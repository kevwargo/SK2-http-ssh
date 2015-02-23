#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include "socket-helpers.h"
#include "httprequest.h"
#include "httpresponse.h"
#include "misc.h"

char *MethodKW[] = {
        "GET ", (char *)HTTP_GET_METHOD,
        "HEAD ", (char *)HTTP_HEAD_METHOD,
        "POST ", (char *)HTTP_POST_METHOD,
        "PUT ", (char *)HTTP_PUT_METHOD,
        "DELETE ", (char *)HTTP_DELETE_METHOD,
        "CONNECT ", (char *)HTTP_CONNECT_METHOD,
        "OPTIONS ", (char *)HTTP_OPTIONS_METHOD,
        "TRACE ", (char *)HTTP_TRACE_METHOD
    };

static int parseHTTPRequestMethod(char *request, char **endptr);
static char *parseHTTPTargetResource(char *request, char **endptr);
static int parseHeaderLine(char *request, HTTPHeaderField *hfield, char **endptr);
static int parseHTTPVersion(char *request, int *major, int *minor, char **endptr);

/* String representation for request method */
char *methodString(int method)
{
    for (int i = 0; i < 8; i++)
        if ((HTTPRequestMethod)MethodKW[i + 1] == method)
            return MethodKW[i];
}

void destroyRequest(HTTPRequest *request)
{
    destroyMessage(&request->message);
    free(request->resource);
    free(request);
}

/* Receiving an HTTP-request from the socket. It only receives the header fields,
   payload is not included */
char *getHTTPRequest(Client *client, char **datastart, int *payloadReceived)
{
    int bufsize = 1024;
    char *buffer = (char *)malloc(bufsize);
    int got = recvUpToPattern(client->sockfd, &buffer, &bufsize, datastart,
                              2, "\r\n\r\n", "\n\n");
    if (got <= 0)
    {
        free(buffer);
        return NULL;
    }
    *payloadReceived = got - (*datastart - buffer);
    return buffer;
}

/* Check if there is a payload coming */
int checkForPayload(Client *client, char *buffer, int received, HTTPRequest *request)
{
    char *contentLengthString;
    if (contentLengthString = getHTTPHeaderValue(&request->message, "content-length"))
    {
        char *endptr;
        long length = strtol(contentLengthString, &endptr, 10);
        if (*endptr != '\0')
            return -1;
        logmsg(client, stdout, "received first %d bytes of payload :)\n", received);
        int remaining = length - received;
        if ((received = recvall(client->sockfd, buffer + received, remaining))
            < remaining)
        {
            logmsg(client, stdout,
                   "ERROR: didn't receive all expected data (%d instead of %d)\n",
                   received, remaining);
            shutdown(client->sockfd, SHUT_RDWR);
            return -2;
        }
        request->message.datalen = length;
        setData(&request->message, buffer, length);
        logmsg(client, stdout, "received all %d bytes of payload\n", length);
        return length;
    }
    else
        return 0;
}

/* Parse byte-stream containing HTTP-request  */
HTTPRequest *parseHTTPRequest(char *buffer)
{
    char *request = buffer;
    int method = parseHTTPRequestMethod(request, &request);
    if (method == HTTP_INVALID_METHOD)
    {
        fprintf(stderr, "method\n");
        return NULL;
    }
    char *resource = parseHTTPTargetResource(request, &request);
    if (! resource)
    {
        fprintf(stderr, "target\n");
        return NULL;
    }
    int major, minor;
    if (parseHTTPVersion(request, &major, &minor, &request) < 0)
    {
        fprintf(stderr, "version\n");
        free(resource);
        return NULL;
    }
    HTTPRequest *result = (HTTPRequest *)malloc(sizeof(HTTPRequest));
    result->method = method;
    result->resource = resource;
    result->message.majorHTTPVersion = major;
    result->message.minorHTTPVersion = minor;
    result->message.headerCount = 0;
    result->message.datalen = 0;
    result->message.data = NULL;
    int hlen = 0;
    int hbufsize = 4;
    result->message.headers = (HTTPHeaderField *)malloc(sizeof(HTTPHeaderField)
                                                        * hbufsize);
    while ((hlen = parseHeaderLine(request, result->message.headers +
                                   result->message.headerCount, &request)) > 0)
    {
        result->message.headerCount++;
        if (hbufsize <= result->message.headerCount)
        {
            hbufsize += 4;
            result->message.headers = realloc(result->message.headers,
                                              sizeof(HTTPHeaderField) * hbufsize);
        }
    }
    if (hlen < 0)
    {
        fprintf(stderr, "error in header %d\n", result->message.headerCount);
        destroyRequest(result);
        return NULL;
    }
    return result;
}

static int parseHTTPRequestMethod(char *request, char **endptr)
{
    int res;    
    for (int i = 0; i < 16; i += 2)
    {
        int len = strlen(MethodKW[i]);
        if (strncmp(request, MethodKW[i], len) == 0)
        {
            if (endptr)
                *endptr = request + len;
            return (HTTPRequestMethod)MethodKW[i + 1];
        }
    }
    return HTTP_INVALID_METHOD;
}

static char *parseHTTPTargetResource(char *request, char **endptr)
{
    char *end = strchr(request, ' ');
    if (! end || end == request)
        return NULL;
    if (endptr)
        *endptr = end + 1;
    return strndup(request, end - request);
}

static int parseHTTPVersion(char *request, int *major, int *minor, char **endptr)
{
    if (strncmp(request, "HTTP/", 5) != 0)
        return -1;
    request += 5; /*point to the first digit*/
    char *end;
    int mjr = (int)strtol(request, &end, 10);
    if (*end != '.')
        return -1;
    request = end + 1;
    int mnr = (int)strtol(request, &end, 10);
    if ( !((*end == '\r' && end[1] == '\n') || *end == '\n') ) // request line must end with CRLF or LF immediatelly after HTTP-version
        return -1;
    *major = mjr;
    *minor = mnr;
    if (endptr)
    {
        if (*end == '\r')
            *endptr = end + 2;
        else
            *endptr = end + 1;
    }
    return 0;
}

static int isvchar(char c)
{
    return c > 32 && c < 127;
}

static int ishvchar(char c)
{
    return isvchar(c) || isblank(c);
}

static int istoken(char c)
{
    return isvchar(c) && ! strchr("(),\"/:;<=>?@[\\]{}", c);
}

static char *findEOL(char *string, int *isdouble)
{
    char *eol;
    char *lf = strchr(string, '\n');
    char *cr = strchr(string, '\r');
    if (! lf)
    {
        printf("! lf\n");
        return NULL;
    }
    if (! cr)
        eol = lf;
    else
    {
        if (lf < cr)
        {
            printf("lf < cr\n");
            eol = lf;
        }
        if (lf - cr == 1)
            eol = cr;
        if (lf - cr > 1)
        {
            printf("lf - cr %ld\n", lf - cr);
            return NULL;
        }
    }
    if (isdouble)
    {
        char *res;
        if (eol == cr)
        {
            res = findEOL(eol + 2, NULL);
            if (res == eol + 2)
                *isdouble = 1;
            else
                *isdouble = 0;
        }
        else
        {
            res = findEOL(eol + 1, NULL);
            if (res == eol + 1)
                *isdouble = 1;
            else
                *isdouble = 0;
        }
    }
    return eol;
}
/* Return value
   1) positive int - length of the parsed string
   2) 0 - end of headers
   3) negative int - an error occured
 */
static int parseHeaderLine(char *request, HTTPHeaderField *hfield, char **endptr)
{
    int isdouble;
    char *eol = findEOL(request, &isdouble);
    if (! eol)
    {
        printf("the very beginning\n");
        return -1;
    }
    if (eol == request)
        if (isdouble)
            return 0;
        else
        {
            printf("start eol\n");
            return -1;
        }
    char *delim = strchr(request, ':');
    if (delim == request)
    {
        printf("no name\n");
        return -1;
    }
    for (char *namechar = request; namechar < delim; namechar++)
        if (! istoken(*namechar))
        {
            printf("invalid char in name\n");
            return -1;
        }
    char *valstart = delim + 1;
    char *valend = eol;
    if (valstart >= valend)
    {
        printf("no value\n");
        return -1;
    }
    while (isblank(*valstart))
        valstart++;
    while (isblank(valend[-1]))
        valend--;
    if (valstart > valend)
    {
        printf("only spaces\n");
        return -1;
    }
    int valsize = valend - valstart;
    for (int valchar = 0; valchar < valsize; valchar++)
        if (! ishvchar(valstart[valchar]))
        {
            printf("invalid char in value\n");
            return -1;
        }
    char *hvalue = strndup(valstart, valsize);
    /* parsing folded header value */
    if (! isdouble) /* if newline was not double */
    {
        eol += strspn(eol, "\r\n");
        valstart = eol;
        /* valstart += strspn(valstart, "\r\n"); */
        while (isblank(*valstart))
        {
            printf("was blank\n");
            eol = findEOL(valstart, &isdouble);
            if (! eol)
            {
                printf("fold: no eol\n");
                return -1;
            }
            valend = eol;
            while (isblank(*valstart))
                valstart++;
            while (isblank(valend[-1]))
                valend--;
            if (valstart > valend)
            {
                printf("fold: only spaces\n");
                return -1;
            }
            for (int valchar = 0; valchar < valend - valstart; valchar++)
                if (! ishvchar(valstart[valchar]))
                {
                    printf("fold: invalid value\n");
                    free(hvalue);
                    return -1;
                }
            hvalue = realloc(hvalue, valsize + valend - valstart + 2 /*delimiting space and null-byte*/);
            hvalue[valsize] = ' ';
            strncpy(hvalue + valsize + 1, valstart, valend - valstart);
            if (isdouble)
                break;
            valsize += valend - valstart + 1;
            hvalue[valsize] = '\0';
            valstart = eol;
            valstart += strspn(valstart, "\r\n");
        }
    }
    char *hname = strndup(request, delim - request);
    hfield->name = hname;
    hfield->value = hvalue;
    if (endptr)
        *endptr = eol;
    return eol - request;
}



#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include "clientlist.h"
#include "httprequest.h"
#include "httpresponse.h"
#include "misc.h"

static int handleGet(Client *client, HTTPRequest *request)
{
    int result;
    char *path = joinpath(client->rootdir, request->resource);
    if (! path)
    {
        printf("%s not found\n", request->resource);
        result = sendHTTPNotFound(client, request->resource);
    }
    else
    {
        int len = strlen(client->rootdir);
        if (strncmp(client->rootdir, path, len) == 0)
        {
            /* if requested resource is in shared directory */
            struct stat filestat;
            stat(path, &filestat);
            if (S_ISDIR(filestat.st_mode))
            {
                free(client->workingSubdir);
                client->workingSubdir = (char *)malloc(strlen(path));
                /* whole path is definitely bigger than subdir
                   so there will not be an overrun */
                if (path[len] == '\0')
                    sprintf(client->workingSubdir, "/");
                else
                    sprintf(client->workingSubdir, "%s", path + len);
                result = sendHTTPFileList(client);
            }
            else if (S_ISREG(filestat.st_mode))
            {
                printf("sending file %s\n", path);
                result = sendHTTPFile(client, path, 1);
                if (result < -1) // some error with file
                {
                    printf("failed to send %s\n", path);
                    int bufsize = strlen("Error while opening file %s: %s") +
                        strlen(path) + 128 /*for errmsg*/;
                    char *message = (char *)malloc(bufsize);
                    message[0] == '\0'; // for strcat to work
                    strcat(message, "Error while ");
                    switch (result)
                    {
                        case -2:
                            strcat(message, "opening file ");
                            break;
                        case -3:
                            strcat(message, "reading file ");
                            break;
                    }
                    strcat(message, path);
                    strcat(message, ": ");
                    int msglen = strlen(message);
                    char *r = strerror_r(errno, message + msglen, bufsize - msglen);
                    if (r != message + msglen)
                        strcpy(message + msglen, r);
                    result = sendHTTPInternalServerError(client, message);
                    free(message);
                }
            }
        }
        else
        {
            printf("forbidden %s %s\n", client->rootdir, path);
            result = sendHTTPForbidden(client);
        }
    }
    free(path);
    return result;
}

int HTTPHandleRequest(HTTPRequest *request, Client *client)
{
    switch (request->method)
    {
        case HTTP_GET_METHOD:
            logmsg(client, stdout, "get method\n");
            break;
        case HTTP_INVALID_METHOD:
            logmsg(client, stdout, "invalid request\n");
            return sendHTTPBadRequest(client, NULL);
    }
    logmsg(client, stdout, "target: %s\n", request->resource);
    logmsg(client, stdout, "HTTP version: %d.%d\n", request->message.majorHTTPVersion,
           request->message.minorHTTPVersion);
    char *host = getHTTPHeaderValue(&request->message, "host");
    if (! host)
    {
        logmsg(client, stdout, "Host header not found\n");
        return sendHTTPBadRequest(client, "");
    }
    for (int i = 0; i < request->message.headerCount; i++)
        logmsg(client, stdout, "Header %d: %s = %s\n", i,
               request->message.headers[i].name,
               request->message.headers[i].value);
    int result;
    if (request->method == HTTP_GET_METHOD)
        result = handleGet(client, request);
    else if (request->method == HTTP_INVALID_METHOD)
        result = sendHTTPBadRequest(client, "");
    else
        result = sendHTTPNotImplemented(client, methodString(request->method));
    char *connection = getHTTPHeaderValue(&request->message, "connection");
    if (connection && strcmp(connection, "close") == 0)
        shutdown(client->sockfd, SHUT_RDWR);
    return result;
}

int httpServerMainLoop(Client *client)
{
    while (1)
    {
        logmsg(client, stdout, "Waiting for request...\n");
        char *requestbuf = getHTTPRequest(client->sockfd);
        if (! requestbuf)
        {
            if (errno == 0)
                return 0;
            return 1;
        }
        /* logmsg(client, stdout, "\n%s\n", requestbuf); */
        HTTPRequest *request = parseHTTPRequest(requestbuf);
        free(requestbuf);
        if (! request)
        {
            logmsg(client, stdout, "Syntax error in request\n");
            sendHTTPBadRequest(client, NULL);
        }
        else
        {
            int res = HTTPHandleRequest(request, client);
            destroyRequest(request);
            if (res < 0)
                return 2;
        }
    }
}


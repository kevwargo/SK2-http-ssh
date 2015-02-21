#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "socket-helpers.h"
#include "misc.h"
#include "clientlist.h"
#include "httpresponse.h"

/* ARGS: title, body */
static char *HTMLTemplate = "<html>\n"
    "<head><title>%s</title></head>\n"
    "<body>\n"
    "%s\n"
    "</body>\n"
    "</html>\n";

/* ARGS: request text */
static char *HTMLBadRequest = "<h1 align=\"center\">"
    "Bad request was sent</h1>\n"
    "<hr/>\n"
    "<h3>Request:</h3>\n"
    "<pre>\n"
    "%s"
    "</pre>\n";

/* ARGS: resource, date */
static char *HTMLNotFound = "<h1 align=\"center\">"
    "Requested resource %s not found</h1>\n"
    "<hr/>\n"
    "<h2 align=\"center\">Current GMT date: %s</h2>\n"
    "<hr/>\n";

static char *HTMLForbidden = "<h1 align=\"center\">"
    "403 Forbidden</h1>\n"
    "<hr/>\n";

static char *HTMLNotImplemented = "<h1 align=\"center\">"
    "Server error: method %s is not implemented on server</h1>\n"
    "<hr/>\n";

static char *HTMLInternalServerError = "<h1 align=\"center\">"
    "An internl server error occured:</h1>\n"
    "<pre>%s</pre>\n"
    "<hr/>\n";

/* ARGS: directory, optional slash */
static char *HTMLFileListHeader = "<h1>Index of %s%s</h1>\n"
    "<hr/>\n"
    "<pre>\n";

static char *HTMLFileListTail = "</pre>\n<hr/>\n";

/* ARGS: filename, optional slash, filename representation, optional slash */
static char *HTMLFileListEntryTemplate = "<a href=\"%s%s\">%s%s</a>\n";

void destroyResponse(HTTPResponse *response)
{
    destroyMessage(&response->message);
    free(response->reasonPhrase);
    free(response);
}

void setReasonPhrase(HTTPResponse *response, char *reasonPhrase)
{
    response->reasonPhrase = strdup(reasonPhrase);
}

HTTPResponse *buildDefaultResponse(int statusCode, char *reasonPhrase)
{
    HTTPResponse *response = (HTTPResponse *)calloc(1, sizeof(HTTPResponse));
    response->message.majorHTTPVersion = 1;
    response->message.minorHTTPVersion = 1;
    response->statusCode = statusCode;
    response->message.data = NULL;
    response->message.datalen = 0;
    setReasonPhrase(response, reasonPhrase);
    setHTTPHeaderValue(&response->message, "Server", "lepecbeke");
    setHTTPHeaderValue(&response->message, "Date", curdate());
    return response;
}

char *encodeResponse(HTTPResponse *response, int *sizeptr)
{
    char *headers = headersToString(&response->message);
    int size = strlen(headers) + strlen(response->reasonPhrase) + strlen("HTTP/") +
        5 /*http-version*/ + 3 /*response code*/ + 6 /*CRLF's and spaces*/ +
        response->message.datalen;
    char *string = (char *)malloc(size);
    size = sprintf(string, "HTTP/%d.%d %d %s\r\n%s\r\n",
                   response->message.majorHTTPVersion,
                   response->message.minorHTTPVersion, response->statusCode,
                   response->reasonPhrase, headers);
    if (response->message.datalen > 0)
        memcpy(string + size, response->message.data, response->message.datalen);
    if (sizeptr)
        *sizeptr = size + response->message.datalen;
    free(headers);
    return string;
}

int sendHTTPGeneral(Client *client, int statusCode, char *reasonPhrase,
                    char *title, char *body)
{
    HTTPResponse *response = buildDefaultResponse(statusCode, reasonPhrase);
    if (title && body)
    {
        char *html = (char *)malloc(strlen(HTMLTemplate) + strlen(title) + strlen(body));
        int size = sprintf(html, HTMLTemplate, title, body);
        response->message.datalen = size;
        setData(&response->message, html, size);
        free(html);
        setHTTPHeaderValue(&response->message, "Content-Type", "text/html; charset=utf-8");
        setHTTPHeaderNumberValue(&response->message, "Content-Length", size);
    }
    char *buffer = encodeResponse(response, NULL);
    int result = sendall(client->sockfd, buffer, strlen(buffer));
    printf("sendhttpgeneral %d\n", result);
    destroyResponse(response);
    free(buffer);
    return result;
}

int sendHTTPBadRequest(Client *client, char *request)
{
    if (! request)
        request = "";
    char *body = (char *)malloc(strlen(HTMLBadRequest) + strlen(request));
    sprintf(body, HTMLBadRequest, request);
    char *title = "Bad request";
    int result = sendHTTPGeneral(client, 400, title, title, body);
    free(body);
    return result;
}

int sendHTTPNotFound(Client *client, char *resource)
{
    char *date = curdate();
    char *body = (char *)malloc(strlen(HTMLNotFound) +
                                strlen(resource) + strlen(date));
    sprintf(body, HTMLNotFound, resource, date);
    char *title = "Not found";
    int result = sendHTTPGeneral(client, 404, title, title, body);
    free(body);
    return result;
}

int sendHTTPForbidden(Client *client)
{
    char *title = "Forbidden";
    return sendHTTPGeneral(client, 403, title, title, HTMLForbidden);
}

int sendHTTPNotImplemented(Client *client, char *method)
{
    char *body = (char *)malloc(strlen(HTMLNotImplemented) +
                                strlen(method));
    sprintf(body, HTMLNotImplemented, method);
    char *title = "Not implemented";
    int result = sendHTTPGeneral(client, 501, title, title, body);
    free(body);
    return result;
}

int sendHTTPInternalServerError(Client *client, char *message)
{
    char *body = (char *)malloc(strlen(HTMLInternalServerError) +
                                strlen(message));
    sprintf(body, HTMLInternalServerError, message);
    char *title = "Internal server error";
    int result = sendHTTPGeneral(client, 500, title, title, body);
    free(body);
    return result;
}

static char *allocHTMLFileListBuffer(char *dirname, char *subdir)
{
    int size = strlen(HTMLFileListHeader);
    size += strlen(HTMLFileListTail);
    size += strlen(subdir);
    DIR *dirp = opendir(dirname);
    struct dirent *entry;
    while (entry = readdir(dirp))
    {
        if (strcmp(entry->d_name, ".") == 0)
            continue;
        size += strlen(HTMLFileListEntryTemplate);
        size += strlen(entry->d_name) * 2;
    }
    return (char *)malloc(size);
}

/* THE RETURN VALUE MUST BE FREED !!! */
char *generateHTMLFileList(char *rootdir, char *subdir)
{
    char *dirname = joinpath(rootdir, subdir); // free
    char *htmlfilelist = allocHTMLFileListBuffer(dirname, subdir); // free
    struct dirent **entries; //free
    int count = scandir(dirname, &entries, NULL, alphasort);
    free(dirname);
    int size = sprintf(htmlfilelist, HTMLFileListHeader, subdir,
                       strcmp(subdir, "/") ? "/" : "");
    for (int i = 0; i < count; i++)
    {
        if (strcmp(entries[i]->d_name, ".") == 0)
            continue;
        if (strcmp(entries[i]->d_name, "..") == 0 &&
            strcmp(subdir, "/") == 0)
            continue;
        int isdir = entries[i]->d_type == DT_DIR;
        size += sprintf(htmlfilelist + size, HTMLFileListEntryTemplate,
                        entries[i]->d_name, isdir ? "/" : "",
                        entries[i]->d_name, isdir ? "/" : "");
        free(entries[i]);
    }
    free(entries);
    strcpy(htmlfilelist + size, HTMLFileListTail);
    size += strlen(HTMLFileListTail);
    char *html_tmpl = (char *)malloc(strlen(HTMLTemplate) + strlen("Index of %s")); //free
    size += sprintf(html_tmpl, HTMLTemplate, "Index of %s", "%s");
    char *html = (char *)malloc(strlen(html_tmpl) + strlen(subdir) +
                                strlen(htmlfilelist));
    int n = sprintf(html, html_tmpl, subdir, htmlfilelist);
    free(htmlfilelist);
    free(html_tmpl);
    return html;
}

int sendHTTPFileList(Client *client)
{
    char *html = generateHTMLFileList(client->rootdir, client->workingSubdir);
    int datalen = strlen(html);
    HTTPResponse *response = buildDefaultResponse(200, "OK");
    response->message.datalen = datalen;
    setData(&response->message, html, datalen);
    setHTTPHeaderValue(&response->message, "Content-Type", "text/html; charset=utf-8");
    setHTTPHeaderNumberValue(&response->message, "Content-Length", datalen);
    char *buffer = encodeResponse(response, NULL);
    int status = sendall(client->sockfd, buffer, strlen(buffer));
    destroyResponse(response);
    free(buffer);
    free(html);
    printf("sendhttpfilelist %d\n", status);
    return status;
}

static HTTPResponse *buildFileResponse(int fd, int withdata, char *buffer, int bufsize)
{
    long filesize = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    HTTPResponse *response = buildDefaultResponse(200, "OK");
    setHTTPHeaderNumberValue(&response->message, "Content-length", filesize);
    setHTTPHeaderValue(&response->message, "Content-type",
                       "application/octet-stream");
    if (withdata)
    {
        int bytes = read(fd, buffer, bufsize);
        if (bytes < 0)
        {
            destroyResponse(response);
            return NULL;
        }
        response->message.datalen = bytes;
        setData(&response->message, buffer, bytes);
    }
    return response;
}

int sendHTTPFile(Client *client, char *filename, int withdata)
{
    int total;
    int fd = open(filename, O_RDONLY);
    if (fd < 0)
        return -2;
    int bufsize = 262144;
    char *buffer = (char *)malloc(bufsize);
    HTTPResponse *response = buildFileResponse(fd, withdata, buffer, bufsize);
    if (! response)
        total = -3;
    else
    {
        int bytes;
        int responselen;
        char *responsebuf = encodeResponse(response, &responselen);
        bytes = sendall(client->sockfd, responsebuf, responselen);
        free(responsebuf);
        destroyResponse(response);
        if (bytes < 0)
            total = -1;
        else
        {
            total += bytes;
            if (withdata)
            {
                while ((bytes = read(fd, buffer, bufsize)) > 0)
                {
                    int sent = sendall(client->sockfd, buffer, bytes);
                    if (sent < bytes)
                    {
                        free(buffer);
                        close(fd);
                        /* printf("tut\n"); */
                        return -1;
                    }
                    /* printf("sent chunk %d\n", sent); */
                    total += sent;
                }
                if (bytes < 0)
                    total = -3;
            }
        }
    }
    free(buffer);
    close(fd);
    return total;
}


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
#include "ssh.h"

static int getLocalResource(Client *client, HTTPRequest *request, char *localresource)
{
    int result;
    char *path = joinpath(client->rootdir, localresource);
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
                result = sendHTTPFileList(client, request->method == HTTP_GET_METHOD);
            }
            else if (S_ISREG(filestat.st_mode))
            {
                printf("sending file %s\n", path);
                result = sendHTTPFile(client, path, request->method == HTTP_GET_METHOD);
                printf("sent %d bytes\n", result);
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
            result = sendHTTPForbidden(client, "Access to this resource is forbidden.");
        }
    }
    free(path);
    return result;
}

static int handleGetHead(Client *client, HTTPRequest *request)
{
    char *resource = request->resource;
    if (strncmp(resource, "/local/", 7) == 0)
        return getLocalResource(client, request, resource + 7);
    else if (strncmp(resource, "/ssh/", 5) == 0)
        return getSSHResource(client, request, resource + 5);
    else if (strcmp(resource, "/") == 0)
    {
        char *path = joinpath(client->rootdir, "index.html");
        int result = sendHTTPFile(client, path, request->method == HTTP_GET_METHOD);
        free(path);
        return result;
    }
    else
        return sendHTTPNotFound(client, resource);
}

static int handlePost(Client *client, HTTPRequest *request)
{
    char *content_type = getHTTPHeaderValue(&request->message, "content-type");
    if (! content_type)
        return sendHTTPBadRequest(client, "content-type");
    char *saveptr;
    char *type = strtok_r(content_type, " ;", &saveptr);
    if (strcmp(request->resource, "/local/ssh-connect.html") == 0)
    {
        char *host = NULL;
        char *username = NULL;
        char *password = NULL;
        if (strcmp(type, "multipart/form-data") == 0)
        {
            char *attr = strtok_r(NULL, " ;=", &saveptr);
            while (strcmp(attr, "boundary") != 0)
            {
                attr = strtok_r(NULL, "=", &saveptr);
                attr = strtok_r(NULL, " ;=", &saveptr);
                if (! attr)
                    return sendHTTPBadRequest(client, "multipart no type");
            }
            char *value = strtok_r(NULL, " ;=", &saveptr);
            if (! value)
                return sendHTTPBadRequest(client, "multipart no value");
            char *delim = (char *)malloc(128);
            char *close_delim = (char *)malloc(128);
            sprintf(delim, "--%s\r\n", value);
            sprintf(close_delim, "--%s--", value);
            char *start = memmem(request->message.data, request->message.datalen,
                                 delim, strlen(delim));
            if (! start)
            {
                free(delim);
                free(close_delim);
                return sendHTTPBadRequest(client, "multipart no start");
            }
            start += strlen(delim);
            char *end = memmem(request->message.data, request->message.datalen,
                               close_delim, strlen(close_delim));
            if (! end)
            {
                free(delim);
                free(close_delim);
                return sendHTTPBadRequest(client, "multipart no end");
            }
            char *partend;
            do
            {
                partend = memmem(start, end - start, delim, strlen(delim));
                if (! partend)
                    partend = end;
                logmsg(client, stdout, "body part:\n");
                fwrite(start, 1, partend - start, stdout);
                fputc('\n', stdout);
                logmsg(client, stdout, "body part end\n");
                start = partend + strlen(delim);
            }
            while (partend != end);
            free(delim);
            free(close_delim);
        }
        else if (strcmp(type, "application/x-www-form-urlencoded") == 0)
        {
            char *string = (char *)malloc(request->message.datalen + 1);
            memcpy(string, request->message.data, request->message.datalen);
            string[request->message.datalen] = '\0';
            char *name = NULL;
            char *value = NULL;
            char *saveptr;
            name = strtok_r(string, "=&", &saveptr);
            while (name)
            {
                value = strtok_r(NULL, "=&", &saveptr);
                if (! value)
                    value = "";
                if (strcmp(name, "host") == 0)
                    host = decodeURL(value);
                if (strcmp(name, "username") == 0)
                    username = decodeURL(value);
                if (strcmp(name, "password") == 0)
                    password = decodeURL(value);
                name = strtok_r(NULL, "=&", &saveptr);
            }
            free(string);
        }
        logmsg(client, stdout, "'%s' '%s' '%s'\n", host, username, password);
        if (host && username && password)
        {
            logmsg(client, stdout, "launching sshConnect with %s %s %s...\n",
                   host, username, password);
            int result = sshConnect(client, host, username, password);
            free(host);
            free(username);
            free(password);
            return result;
        }
        return sendHTTPBadRequest(client, "final");
    }
    else
        return sendHTTPBadRequest(client, "some error");
}

int HTTPHandleRequest(HTTPRequest *request, Client *client)
{
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
    logmsg(client, stdout, "payload: \n");
    fwrite(request->message.data, 1, request->message.datalen, stdout);
    fputc('\n', stdout);
    logmsg(client, stdout, "end of payload\n");
    int result;
    switch (request->method)
    {
        case HTTP_GET_METHOD:
            printf("GET request\n");
            result = handleGetHead(client, request);
            break;
        case HTTP_HEAD_METHOD:
            printf("HEAD request\n");
            result = handleGetHead(client, request);
            break;
        case HTTP_POST_METHOD:
            printf("POST request\n");
            result = handlePost(client, request);
            break;
        case HTTP_INVALID_METHOD:
            printf("INVALID request\n");
            result = sendHTTPBadRequest(client, "");
            break;
        default:
            printf("request %s is not implemented\n", methodString(request->method));
            result = sendHTTPNotImplemented(client, methodString(request->method));
            break;
    }
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
        char *datastart;
        int payloadReceived;
        char *requestbuf = getHTTPRequest(client, &datastart, &payloadReceived);
        if (! requestbuf)
        {
            if (errno == 0)
                return 0;
            return 1;
        }
        /* logmsg(client, stdout, "\n%s\n", requestbuf); */
        HTTPRequest *request = parseHTTPRequest(requestbuf);
        if (! request)
        {
            logmsg(client, stdout, "Syntax error in request\n");
            sendHTTPBadRequest(client, requestbuf);
        }
        else
        {
            int size = checkForPayload(client, datastart, payloadReceived, request);
            switch (size)
            {
                case -1:
                    sendHTTPBadRequest(client, requestbuf);
                    logmsg(client, stdout, "ERROR -1\n");
                    break;
                case -2:
                    free(requestbuf);
                    destroyRequest(request);
                    logmsg(client, stdout, "ERROR -2\n");
                    return 2;
                case 0:
                    logmsg(client, stdout, "No payload\n");
                    break;
                default:
                    logmsg(client, stdout, "Received payload of size %d\n", size);
                    break;
            }
            int res = HTTPHandleRequest(request, client);
            if (res < 0)
            {
                free(requestbuf);
                return 2;
            }
            destroyRequest(request);
        }
        free(requestbuf);
    }
}


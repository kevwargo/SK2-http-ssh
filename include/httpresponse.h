#ifndef __HTTPRESPONSE_H_INCLUDED_
#define __HTTPRESPONSE_H_INCLUDED_

#include "httpmessage.h"
#include "clientlist.h"

typedef struct {
    int statusCode;
    char *reasonPhrase;
    HTTPMessage message;
} HTTPResponse;


extern int sendHTTPBadRequest(Client *client, char *request);
extern int sendHTTPNotFound(Client *client, char *resource);
extern int sendHTTPForbidden(Client *client);
extern int sendHTTPNotImplemented(Client *client, char *method);
extern int sendHTTPInternalServerError(Client *client, char *message);
extern int sendHTTPFileList(Client *client);
extern int sendHTTPFile(Client *client, char *filename, int withdata);

#endif

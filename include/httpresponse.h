#ifndef __HTTPRESPONSE_H_INCLUDED_
#define __HTTPRESPONSE_H_INCLUDED_

#include "httpmessage.h"
#include "clientlist.h"

typedef struct {
    int statusCode;
    char *reasonPhrase;
    HTTPMessage message;
} HTTPResponse;


extern char *HTMLTemplate;
extern char *HTMLBadRequest;
extern char *HTMLNotFound;
extern char *HTMLForbidden;
extern char *HTMLNotImplemented;
extern char *HTMLInternalServerError;
extern char *HTMLBadGateway;
extern char *HTMLFileListHeader;
extern char *HTMLFileListTail;
extern char *HTMLFileListEntryTemplate;

extern void destroyResponse(HTTPResponse *response);
extern char *encodeResponse(HTTPResponse *response, int *sizeptr);
extern HTTPResponse *buildDefaultResponse(int statusCode, char *reasonPhrase);
extern long long sendHTTPGeneral(Client *client, int statusCode, char *reasonPhrase,
                                 char *title, char *body);
extern long long sendHTTPBadRequest(Client *client, char *request);
extern long long sendHTTPNotFound(Client *client, char *resource);
extern long long sendHTTPForbidden(Client *client, char *reason);
extern long long sendHTTPNotImplemented(Client *client, char *method);
extern long long sendHTTPInternalServerError(Client *client, char *message);
extern long long sendHTTPBadGateway(Client *client, char *reason);
extern long long sendHTTPFileList(Client *client, char *html, int withdata);
extern long long sendHTTPLocalFileList(Client *client, int withdata);
extern long long sendHTTPFile(Client *client, char *filename, int withdata);

#endif

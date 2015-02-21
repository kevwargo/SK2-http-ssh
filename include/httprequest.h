#ifndef __HTTPREQUEST_H_INCLUDED_
#define __HTTPREQUEST_H_INCLUDED_

#include "httpmessage.h"

typedef enum {
    HTTP_GET_METHOD,
    HTTP_HEAD_METHOD,
    HTTP_POST_METHOD,
    HTTP_PUT_METHOD,
    HTTP_DELETE_METHOD,
    HTTP_CONNECT_METHOD,
    HTTP_OPTIONS_METHOD,
    HTTP_TRACE_METHOD,
    HTTP_INVALID_METHOD
} HTTPRequestMethod;

typedef struct {
    HTTPRequestMethod method;
    char *resource;
    HTTPMessage message;
} HTTPRequest;

extern char *MethodKW[];

extern char *getHTTPRequest(int sockfd);
extern HTTPRequest *parseHTTPRequest(char *buffer);
extern void destroyRequest(HTTPRequest *request);
extern char *methodString(int method);

#endif

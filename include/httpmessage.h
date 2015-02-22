#ifndef __HTTPMESSAGE_H_INCLUDED_
#define __HTTPMESSAGE_H_INCLUDED_

typedef struct {
    char *name;
    char *value;
} HTTPHeaderField;

typedef struct {
    int majorHTTPVersion;
    int minorHTTPVersion;
    int headerCount;
    HTTPHeaderField *headers;
    long long datalen;
    char *data;
} HTTPMessage;

extern void destroyMessage(HTTPMessage *message);
extern char *getHTTPHeaderValue(HTTPMessage *message, char *hname);
extern void setHTTPHeaderValue(HTTPMessage *message, char *hname, char *hvalue);
extern void setHTTPHeaderNumberValue(HTTPMessage *message, char *hname, long long number);
extern void setData(HTTPMessage *message, char *data, int len);
extern char *headersToString(HTTPMessage *message);

#endif

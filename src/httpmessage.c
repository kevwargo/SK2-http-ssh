#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "httpmessage.h"

void destroyMessage(HTTPMessage *message)
{
    for (int i = 0; i < message->headerCount; i++)
    {
        free(message->headers[i].name);
        free(message->headers[i].value);
    }
    free(message->headers);
    if (message->data)
        free(message->data);
}

char *getHTTPHeaderValue(HTTPMessage *message, char *hname)
{
    for (int i = 0; i < message->headerCount; i++)
        if (strcasecmp(message->headers[i].name, hname) == 0)
            return message->headers[i].value;
    return NULL;
}

void setHTTPHeaderValue(HTTPMessage *message, char *hname, char *hvalue)
{
    for (int i = 0; i < message->headerCount; i++)
        if (strcasecmp(message->headers[i].name, hname) == 0)
        {
            message->headers[i].value = strdup(hvalue);
            return;
        }
    int count = message->headerCount;
    /* printf("before realloc %llu %lu\n", (unsigned long long)message->headers, */
           /* (pos + 1) * sizeof(HTTPHeaderField)); */
    message->headers = (HTTPHeaderField *)realloc(message->headers,
                                                  (count + 1) * sizeof(HTTPHeaderField));
    /* printf("after realloc\n"); */
    message->headers[count].name = strdup(hname);
    message->headers[count].value = strdup(hvalue);
    message->headerCount++;
}

void setHTTPHeaderNumberValue(HTTPMessage *message, char *hname, long long number)
{
    char *value = (char *)malloc(32);
    sprintf(value, "%lld", number);
    setHTTPHeaderValue(message, hname, value);
}

void setData(HTTPMessage *message, char *data, int len)
{
    message->data = (char *)malloc(len);
    memcpy(message->data, data, len);
}

char *headersToString(HTTPMessage *message)
{
    int size = 0;
    for (int i = 0; i < message->headerCount; i++)
    {
        size += strlen(message->headers[i].name);
        size += strlen(message->headers[i].value);
        size += 4; // colon, space, CRLF
    }
    size += 1; //null-byte
    char *string = (char *)malloc(size);
    int pos = 0;
    for (int i = 0; i < message->headerCount; i++)
        pos += sprintf(string + pos, "%s: %s\r\n", message->headers[i].name,
                       message->headers[i].value);
    return string;
}

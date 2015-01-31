#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include "socket-helpers.h"


char *getHTTPRequest(int sockfd)
{
    int bufsize = 1024;
    char *buffer = (char *)malloc(bufsize);
    char *request_end = NULL;
    int total = 0;
    do {
        if (total == bufsize)
        {
            bufsize += 1024;
            buffer = realloc(buffer, bufsize);
        }
        int length = recv(sockfd, buffer + total, bufsize - total - 1, 0);
        if (length < 0)
        {
            free(buffer);
            return NULL;
        }
        if (length == 0)
        {
            free(buffer);
            errno = 0;
            return NULL;
        }
        buffer[total + length] = '\0';
        request_end = strstr(buffer + total, "\r\n\r\n");
        total += length;
        /* printf("%d %llu\n", total, (unsigned long long) request_end); */
    } while (request_end == NULL);
    return buffer;
}


#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include "socket-helpers.h"

int parseSocketAddress(struct sockaddr_in *sockaddr, char *ipaddr, char *strport)
{
    if (sockaddr == NULL)
        return ADDRERR_SOCKADDR_NULL;
    if (ipaddr == NULL)
        return ADDRERR_IP_NULL;
    if (strport == NULL)
        return ADDRERR_PORT_NULL;
    memset(sockaddr, '\0', sizeof(struct sockaddr_in));
    if (! inet_aton(ipaddr, &(sockaddr->sin_addr)))
        return ADDRERR_INVALID_IP;
    char *endptr;
    long int port = strtol(strport, &endptr, 0);
    if (*strport == '\0' || *endptr != '\0')
        return ADDRERR_INVALID_PORT;
    if (port > 65535 || port < 0)
        return ADDRERR_INVALID_PORT;
    sockaddr->sin_port = htons((unsigned short)port);
    sockaddr->sin_family = AF_INET;
    return 0;
}

int printAddressError(int errcode)
{
    if (errcode == 0)
        return 0;
    fprintf(stderr, "Error %d: ", errcode);
    switch (errcode)
    {
        case ADDRERR_SOCKADDR_NULL:
            fprintf(stderr, "string that must contain sockaddr structure is NULL");
            break;
        case ADDRERR_IP_NULL:
            fprintf(stderr, "string that must contain IP address is NULL\n");
            break;
        case ADDRERR_PORT_NULL:
            fprintf(stderr, "string that must contain port number is NULL\n");
            break;
        case ADDRERR_INVALID_IP:
            fprintf(stderr, "invalid IP address\n");
            break;
        case ADDRERR_INVALID_PORT:
            fprintf(stderr, "invalid port number\n");
            break;
    }
    return errcode;
}

void exitOnError(int errcode)
{
    if (errcode == 0)
        return;
    exit(errcode);
}

int sendall(int socket, void *buffer, size_t length)
{
    char *ptr = (char *)buffer;
    int total = 0;
    while (length > 0)
    {
        int chunksize = send(socket, ptr, length, 0);
        /* printf("sent in sendall %d\n", chunksize); */
        if (chunksize < 0)
        {
            perror("IN SENDALL");
            return chunksize;
        }
        total += chunksize;
        ptr += chunksize;
        length -= chunksize;
    }
    return total;
}

int recvUpToPattern(int sockfd, char **bufptr, int *bufsizeptr, char **endptr,
                    int ptrncnt, ...)
{
    char *found = NULL;
    int total = 0;
    char *end;
    if (! endptr)
        endptr = &end;
    if (! bufsizeptr || ! bufptr)
    {
        errno = EINVAL;
        return -1;
    }
    va_list patterns;
    if (! *bufptr || *bufsizeptr == 0)
    {
        *bufsizeptr = 1024;
        *bufptr = (char *)malloc(*bufsizeptr);
    }
    do {
        if (total == *bufsizeptr - 1) // considering terminating NULL-byte
        {
            *bufsizeptr += 1024;
            *bufptr = realloc(*bufptr, *bufsizeptr);
        }
        // same NULL-byte consideration
        int chunksize = recv(sockfd, *bufptr + total, *bufsizeptr - total - 1, 0);
        if (chunksize < 0)
        {
            va_end(patterns);
            return chunksize;
        }
        if (chunksize == 0)
        {
            va_end(patterns);
            errno = 0;
            return 0;
        }
        (*bufptr)[total + chunksize] = '\0';
        va_start(patterns, ptrncnt);
        for (int i = 0; i < ptrncnt; i++)
        {
            char *pattern = va_arg(patterns, char *);
            int ptrnlen = strlen(pattern);
            int offset = total - ptrnlen;
            if (offset < 0)
                offset = 0;
            found = strstr(*bufptr + offset, pattern);
            if (found)
            {
                *endptr = found;
                break;
            }
        }
        total += chunksize;
    } while (! found);
    va_end(patterns);
    return total;
}

int recvall(int socket, char *buffer, int length)
{
    int total = 0;
    while (total < length)
    {
        int chunksize = recv(socket, buffer, length - total, 0);
        if (chunksize < 0)
            return chunksize;
        if (chunksize == 0)
            return total;
        total += chunksize;
    }
    return total;
}

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "socket-helpers.h"

int parsesockaddr(struct sockaddr_in *sockaddr, char *ipaddr, char *strport)
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

int printaddrerr(int errcode)
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

void exitonerror(int errcode)
{
    if (errcode == 0)
        return;
    exit(errcode);
}

int sendall(int socket, void *buffer, size_t length)
{
    char *ptr = (char*) buffer;
    while (length > 0)
    {
        int chunksize = send(socket, ptr, length, 0);
        if (chunksize < 0)
            return chunksize;
        ptr += chunksize;
        length -= chunksize;
    }
    return length;
}


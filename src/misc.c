#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <string.h>
#include "clientlist.h"

char *curdate()
{
    time_t t = time(NULL);
    char *date = asctime(gmtime(&t));
    for (int pos = strlen(date) - 1; date[pos] == '\n'; pos--)
        date[pos] = '\0';
    return date;
}

void logmsg(Client *client, FILE *stream, char *fmt, ...)
{
    fprintf(stream, "%s: client %s:%d: ", curdate(), inet_ntoa(client->address.sin_addr),
            ntohs(client->address.sin_port));
    va_list args;
    va_start(args, fmt);
    vfprintf(stream, fmt, args);
    va_end(args);
}

char *joinpath(char *dir, char *base)
{
    int dirlen = strlen(dir);
    int baselen = strlen(base);
    char *fullname = (char *)malloc(dirlen + baselen + 2);
    sprintf(fullname, "%s/%s", dir, base);
    char *result = realpath(fullname, NULL);
    free(fullname);
    return result;
}

char *decodeURL(char *url)
{
    char *result = (char *)malloc(strlen(url) + 1);
    int respos = 0;
    for (int urlpos = 0; url[urlpos]; urlpos++)
    {
        if (url[urlpos] == '+')
            result[respos] = ' ';
        else if (url[urlpos] == '%')
        {
            char hex[3];
            hex[0] = url[++urlpos];
            hex[1] = url[++urlpos];
            hex[2] = '\0';
            result[respos] = (uint8_t)strtol(hex, NULL, 16);
        }
        else
            result[respos] = url[urlpos];
        respos++;
    }
    result[respos] = '\0';
    return result;
}

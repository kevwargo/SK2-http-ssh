#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include "clientlist.h"

void logmsg(Client *client, FILE *stream, char *fmt, ...)
{
    fprintf(stream, "client %s:%d: ", inet_ntoa(client->address.sin_addr),
            ntohs(client->address.sin_port));
    va_list args;
    va_start(args, fmt);
    vfprintf(stream, fmt, args);
    va_end(args);
}

char *curdate()
{
    time_t t = time(NULL);
    char *date = asctime(gmtime(&t));
    for (int pos = strlen(date) - 1; date[pos] == '\n'; pos--)
        date[pos] = '\0';
    return date;
}

#include <stdlib.h>
#include <stdio.h>
#include "clientlist.h"


Client *addClient(Client **clptr, int sockfd)
{
    if (! clptr)
        return NULL;
    Client **cpp;
    for (cpp = clptr; *cpp; cpp = &((*cpp)->next))
        if ((*cpp)->sockfd == sockfd)
            return NULL;
    *cpp = (Client *)malloc(sizeof(Client));
    (*cpp)->sockfd = sockfd;
    (*cpp)->next = NULL;
    printf("new cl %llu\n", (unsigned long long)*cpp);
    return *cpp;
}

void removeClient(Client **clptr, int sockfd)
{
    if (! clptr)
        return;
    for (Client **cpp = clptr; *cpp; cpp = &((*cpp)->next))
        if ((*cpp)->sockfd == sockfd)
        {
            /* printf("removing %d\n", sockfd); */
            Client *next = (*cpp)->next;
            free(*cpp);
            *cpp = next;
            return;
        }
}

void clearClientList(Client **clptr)
{
    if (! clptr)
        return;
    for (Client *cp = *clptr; cp; cp = cp->next)
        removeClient(clptr, cp->sockfd);
}

Client *getClientBySockfd(Client *cl, int sockfd)
{
    if (! cl)
        return NULL;
    for (Client *cp = cl; cp; cp = cp->next)
        if (cp->sockfd == sockfd)
            return cp;
    return NULL;
}

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <libssh2.h>
#include <sys/socket.h>
#include <pthread.h>
#include "clientlist.h"
#include "misc.h"


Client *addClient(Client **clptr, int sockfd)
{
    if (! clptr)
        return NULL;
    Client **cpp;
    for (cpp = clptr; *cpp; cpp = &((*cpp)->next))
        if ((*cpp)->sockfd == sockfd)
            return *cpp;
    *cpp = (Client *)malloc(sizeof(Client));
    (*cpp)->sockfd = sockfd;
    (*cpp)->rootdir = NULL;
    (*cpp)->workingSubdir = NULL;
    (*cpp)->ssh_session = NULL;
    (*cpp)->next = NULL;
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
            shutdown((*cpp)->sockfd, SHUT_RDWR);
            close((*cpp)->sockfd);
            if ((*cpp)->ssh_session)
            {
                libssh2_session_disconnect((*cpp)->ssh_session, "Disconnecting");
                libssh2_session_free((*cpp)->ssh_session);
            }
            pthread_join((*cpp)->thread, NULL);
            logmsg(*cpp, stdout, "Thread terminated\n");
            free((*cpp)->rootdir);
            free((*cpp)->workingSubdir);
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

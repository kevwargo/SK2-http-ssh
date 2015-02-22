#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <libssh2.h>
#include <libssh2_sftp.h>
#include <sys/socket.h>
#include <pthread.h>
#include "clientlist.h"
#include "misc.h"

pthread_mutex_t *ClientListMutex = NULL;


Client *addClient(Client **clptr, int sockfd)
{
    if (! ClientListMutex)
    {
        ClientListMutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
        pthread_mutex_init(ClientListMutex, NULL);
    }
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
    (*cpp)->ssh_host = NULL;
    (*cpp)->ssh_session = NULL;
    (*cpp)->sftp_session = NULL;
    (*cpp)->next = NULL;
    return *cpp;
}

void removeClient(Client **clptr, int sockfd)
{
    if (! ClientListMutex)
    {
        ClientListMutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
        pthread_mutex_init(ClientListMutex, NULL);
    }
    pthread_mutex_lock(ClientListMutex);
    if (! clptr)
    {
        pthread_mutex_unlock(ClientListMutex);
        return;
    }
    for (Client **cpp = clptr; *cpp; cpp = &((*cpp)->next))
        if ((*cpp)->sockfd == sockfd)
        {
            /* printf("removing %d\n", sockfd); */
            Client *next = (*cpp)->next;
            shutdown((*cpp)->sockfd, SHUT_RDWR);
            close((*cpp)->sockfd);
            if ((*cpp)->sftp_session)
            {
                /* logmsg(*cpp, stdout, "before sftp close\n"); */
                libssh2_sftp_shutdown((*cpp)->sftp_session);
            }
            if ((*cpp)->ssh_session)
            {
                /* logmsg(*cpp, stdout, "before ssh disconnect\n"); */
                libssh2_session_disconnect((*cpp)->ssh_session, "Disconnecting");
                /* logmsg(*cpp, stdout, "before ssh free\n"); */
                libssh2_session_free((*cpp)->ssh_session);
                /* logmsg(*cpp, stdout, "after ssh free\n"); */
            }
            /* if (pthread_self() != (*cpp)->thread) */
            /*     pthread_join((*cpp)->thread, NULL); */
            /* logmsg(*cpp, stdout, "Thread terminated\n"); */
            free((*cpp)->ssh_host);
            free((*cpp)->rootdir);
            free((*cpp)->workingSubdir);
            free(*cpp);
            *cpp = next;
            break;
        }
    pthread_mutex_unlock(ClientListMutex);
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

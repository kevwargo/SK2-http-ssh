#ifndef __CLIENTLIST_H_INCLUDED
#define __CLIENTLIST_H_INCLUDED

#include <pthread.h>
#include <netinet/in.h>

typedef struct Client
{
    int sockfd;
    pthread_t thread;
    struct sockaddr_in address;
    struct Client *next;
} Client;

extern Client *addClient(Client **clptr, int sockfd);
extern void removeClient(Client **clptr, int sockfd);
extern void clearClientList(Client **clptr);
extern Client *getClientBySockfd(Client *cl, int sockfd);

#endif
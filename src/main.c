#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include "socket-helpers.h"
#include "clientlist.h"
#include "misc.h"
#include "httpserver.h"


#define QUEUE_SIZE 5
#define BUFSIZE 2048


int ServerSocket;
Client *ClientList = NULL;
char *RootDir = NULL;


void *handleClient(void *arg)
{
    Client *client = (Client *)arg;
    logmsg(client, stdout, "New thread created\n");
    switch (HTTPServerMainLoop(client, RootDir))
    {
        case 0:
            logmsg(client, stdout, "Successfully disconnected.\n");
            break;
        case 1:
            logmsg(client, stderr, "Error receiving data from socket: %s\n",
                strerror_r(errno, NULL, 0));
            break;
        case 2:
            logmsg(client, stderr, "Error sending data to socket: %s\n",
                strerror_r(errno, NULL, 0));
            break;
    }
    shutdown(client->sockfd, SHUT_RDWR);
    close(client->sockfd);
}

int acceptClient(int servsock, Client **clptr, void *(*start_routine) (void *), Client **new)
{
    struct sockaddr_in caddr;
    int sa_size = sizeof(struct sockaddr_in);
    int sockfd = accept(servsock, (struct sockaddr *)&caddr, &sa_size);
    if (sockfd < 0)
        return sockfd;
    Client *client = addClient(clptr, sockfd);
    memcpy(&(client->address), &caddr, sa_size);
    int status = pthread_create(&(client->thread), NULL, start_routine, client);
    if (status != 0)
    {
        removeClient(clptr, sockfd);
        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
    }
    else
        *new = client;
    return status;
}

void cleanup()
{
    printf("cleaning up...\n");
    for (Client *client = ClientList; client; client = client->next)
    {
        shutdown(client->sockfd, SHUT_RDWR);
        close(client->sockfd);
        pthread_join(client->thread, NULL);
        printf("Thread for the client %s:%d terminated\n",
               inet_ntoa(client->address.sin_addr), client->address.sin_port);
    }
    clearClientList(&ClientList);
    close(ServerSocket);
    free(RootDir);
}

void signalHandler(int signum)
{
    psignal(signum, "Caught signal");
    cleanup();
    exit(signum == SIGINT ? 0 : 1);
}

int main(int argc, char **argv)
{
    if (argc < 4)
    {
        fprintf(stderr, "usage: %s HOST PORT ROOT-DIR\n", argv[0]);
        return 1;
    }
    struct sockaddr_in serverSocketAddress;
    ServerSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (ServerSocket < 0)
    {
        perror("Cannot create socket");
        return 1;
    }
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    int enable = 1;
    if (setsockopt(ServerSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0)
    {
        perror("Cannot set socket options");
        close(ServerSocket);
        return 1;
    }
    int errcode = parsesockaddr(&serverSocketAddress, argv[1], argv[2]);
    exitonerror(printaddrerr(errcode));
    if (bind(ServerSocket,
             (struct sockaddr *)&serverSocketAddress,
             sizeof(serverSocketAddress)) < 0)
    {
        perror("Cannot bind socket");
        close(ServerSocket);
        return 1;
    }
    if (listen(ServerSocket, QUEUE_SIZE) < 0)
    {
        perror("Cannot set queue size");
        close(ServerSocket);
        return 1;
    }

    RootDir = realpath(argv[3], NULL);
    if (! RootDir)
    {
        perror("Invalid root directory");
        close(ServerSocket);
        return 1;
    }

    while (1)
    {
        Client *new;
        int status = acceptClient(ServerSocket, &ClientList, handleClient, &new);
        if (status < 0)
            perror("Cannot accept an incoming connection");
        else if (status > 0)
            perror("Cannot start thread for client");
        else
            logmsg(new, stdout, "New connection.\n");
    }
}

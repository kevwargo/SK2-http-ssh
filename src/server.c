#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include "address.h"
#include "clientlist.h"

#define QUEUE_SIZE 5
#define BUFSIZE 2048

int ServerSocket;

Client *ClientList = NULL;

void quit(int signum)
{
    printf("quitting...\n");
    if (signum > 1)
        fprintf(stderr, "Caught signal %d\n", signum);
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
    exit(signum);
}

void *handleClient(void *arg)
{
    Client *client = (Client *)arg;
    printf("New thread for client %s:%d\n",
           inet_ntoa(client->address.sin_addr), client->address.sin_port);
    char *buffer = (char *)malloc(BUFSIZE);
    while (1)
    {
        ssize_t length = recv(client->sockfd, buffer, BUFSIZE - 10, 0);
        if (length == 0)
            return NULL;
        if (length < 0)
        {
            fprintf(stderr, "%s:%d: ", inet_ntoa(client->address.sin_addr),
                    client->address.sin_port);
            perror("Cannot read data from client");
            free(buffer);
            shutdown(client->sockfd, SHUT_RDWR);
            close(client->sockfd);
            return NULL;
        }
        if (buffer[length - 1] =='\n')
            buffer[length - 1] = '\0';
        else
            buffer[length] = '\0';
        strcat(buffer, " received\n");
        ssize_t sent = send(client->sockfd, buffer, length + 10, 0);
        if (sent < 0)
        {
            fprintf(stderr, "%s:%d: ", inet_ntoa(client->address.sin_addr),
                    client->address.sin_port);
            perror("Cannot send data to client");
            free(buffer);
            shutdown(client->sockfd, SHUT_RDWR);
            close(client->sockfd);
            return NULL;
        }
    }
}

int acceptClient(Client **clptr, int servsock, void *(*start_routine) (void *))
{
    struct sockaddr_in caddr;
    int sa_size = sizeof(struct sockaddr_in);
    int sockfd = accept(servsock, (struct sockaddr *)&caddr, &sa_size);
    if (sockfd < 0)
        return sockfd;
    Client *client = addClient(clptr, sockfd);
    memcpy(&(client->address), &caddr, sizeof(struct sockaddr_in));
    int status = pthread_create(&(client->thread), NULL, start_routine, client);
    if (status != 0)
    {
        removeClient(clptr, sockfd);
        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
    }
    return status;
}

int main(int argc, char **argv)
{
    if (argc < 3)
        return 1;
    struct sockaddr_in serverSocketAddress;
    ServerSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (ServerSocket < 0)
    {
        perror("Cannot create socket");
        return 1;
    }
    signal(SIGINT, quit);
    signal(SIGTERM, quit);
    int enable = 1;
    if (setsockopt(ServerSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0)
    {
        perror("Cannot set socket options");
        close(ServerSocket);
        return 1;
    }
    int errcode = setsockaddr(&serverSocketAddress, argv[1], argv[2]);
    exitonerror(printerror(errcode));
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

    while (1)
    {
        int status = acceptClient(&ClientList, ServerSocket, handleClient);
        if (status < 0)
            perror("Cannot accept an incoming connection");
        else if (status > 0)
            fprintf(stderr, "Cannot start thread for client: %s\n", strerror(status));
        else
        {
            Client *last;
            for (last = ClientList; last->next; last = last->next) { }
            printf("Successfully handled connection from %s:%d\n",
                   inet_ntoa(last->address.sin_addr), last->address.sin_port);
        }
    }

}

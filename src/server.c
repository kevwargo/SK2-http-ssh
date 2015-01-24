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
#include "address.h"

#define QUEUE_SIZE 5
#define BUFSIZE 128

int serverSocket;
int clientSocket;

void sighandle(int signum)
{
    fprintf(stderr, "Caught signal %d\n", signum);
    if (shutdown(clientSocket, SHUT_RDWR) < 0)
        perror("Cannot shutdown client connection");
    close(clientSocket);
    close(serverSocket);
}

int main(int argc, char **argv)
{
    if (argc < 3)
        return 1;
    struct sockaddr_in serverSocketAddress;
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0)
    {
        perror("Cannot create socket");
        return 1;
    }
    signal(SIGINT, sighandle);
    signal(SIGTERM, sighandle);
    int enable = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0)
    {
        perror("Cannot set socket options");
        close(serverSocket);
        return 1;
    }
    int errcode = setsockaddr(&serverSocketAddress, argv[1], argv[2]);
    exitonerror(printerror(errcode));
    if (bind(serverSocket,
             (struct sockaddr *)&serverSocketAddress,
             sizeof(serverSocketAddress)) < 0)
    {
        perror("Cannot bind socket");
        close(serverSocket);
        return 1;
    }
    if (listen(serverSocket, QUEUE_SIZE) < 0)
    {
        perror("Cannot set queue size");
        close(serverSocket);
        return 1;
    }

    struct sockaddr_in clientSocketAddress;
    int sa_size = sizeof(clientSocketAddress);
    clientSocket = accept(serverSocket,
                          (struct sockaddr *)&clientSocketAddress,
                          &sa_size);
    if (clientSocket < 0)
    {
        perror("Cannot accept an incoming connection");
        close(serverSocket);
        return 1;
    }

    char *buffer = (char *)malloc(BUFSIZE);
    while (1)
    {
        ssize_t length = recv(clientSocket, buffer, BUFSIZE - 10, 0);
        if (length < 0)
        {
            perror("Cannot read data from client");
            free(buffer);
            if (shutdown(clientSocket, SHUT_RDWR) < 0)
                perror("Cannot shutdown client connection");
            close(clientSocket);
            close(serverSocket);
            return 1;
        }
        if (buffer[length - 1] =='\n')
            buffer[length - 1] = '\0';
        else
            buffer[length] = '\0';
        strcat(buffer, " received\n");
        ssize_t sent = send(clientSocket, buffer, length + 10, 0);
        if (sent < 0)
        {
            perror("Cannot send data to client");
            free(buffer);
            if (shutdown(clientSocket, SHUT_RDWR) < 0)
                perror("Cannot shutdown client connection");
            close(clientSocket);
            close(serverSocket);
            return 1;
        }
    }
}

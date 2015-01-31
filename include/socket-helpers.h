#ifndef __SOCKET_HELPERS_H_INCLUDED_
#define __SOCKET_HELPERS_H_INCLUDED_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#define ADDRERR_SOCKADDR_NULL   1
#define ADDRERR_IP_NULL         2
#define ADDRERR_PORT_NULL       3
#define ADDRERR_INVALID_IP      4
#define ADDRERR_INVALID_PORT    5

extern int parsesockaddr(struct sockaddr_in *sockaddr, char *ipaddr, char *strport);
extern int printaddrerr(int errcode);
extern void exitonerror(int errcode);
extern int sendall(int socket, void *buffer, size_t length);


#endif

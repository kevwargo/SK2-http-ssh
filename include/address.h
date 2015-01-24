#ifndef __ADDRESS_H_INCLUDED_
#define __ADDRESS_H_INCLUDED_

#define ADDRERR_SOCKADDR_NULL   1
#define ADDRERR_IP_NULL         2
#define ADDRERR_PORT_NULL       3
#define ADDRERR_INVALID_IP      4
#define ADDRERR_INVALID_PORT    5

extern int setsockaddr(struct sockaddr_in *sockaddr, char *ipaddr, char *strport);
extern int printerror(int errcode);
extern void exitonerror(int errcode);

#endif

#ifndef __SSH_H_INCLUDED_
#define __SSH_H_INCLUDED_

#include "clientlist.h"
#include "httprequest.h"

extern int ssh_test(int argc, char **argv);
extern int sshConnect(Client *client, char *host, char *username, char *password);
extern int getSSHResource(Client *client, HTTPRequest *request, char *sshrequest);

#endif

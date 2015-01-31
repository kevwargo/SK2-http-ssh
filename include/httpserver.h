#ifndef __HTTPSERVER_H_INCLUDED_
#define __HTTPSERVER_H_INCLUDED_

#include "clientlist.h"


extern int HTTPServerMainLoop(Client *client, char *rootdir);

#endif

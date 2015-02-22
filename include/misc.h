#ifndef __MISC_H_INCLUDED_
#define __MISC_H_INCLUDED_

#include <stdio.h>
#include <stdarg.h>
#include "clientlist.h"

extern void logmsg(Client *client, FILE *stream, char *fmt, ...);
extern char *curdate();
extern char *joinpath(char *dir, char *base, int check);
extern char *decodeURL(char *url);

#endif

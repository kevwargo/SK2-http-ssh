#include <stdlib.h>
#include <errno.h>
#include "clientlist.h"
#include "httprequest.h"
#include "httpresponse.h"
#include "misc.h"


int HTTPServerMainLoop(Client *client, char *rootdir)
{
    while (1)
    {
        logmsg(client, stdout, "Waiting for request...\n");
        char *request = getHTTPRequest(client->sockfd);
        if (! request)
        {
            if (errno == 0)
                return 0;
            return 1;
        }
        logmsg(client, stdout, "Got request: \n%s", request);
        free(request);
        /* if (sendHTTPNotFound(client->sockfd) < 0) */
        if (sendHTTPFileList(client->sockfd, rootdir) < 0)
            return 2;
    } 
}

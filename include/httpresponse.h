#ifndef __HTTPRESPONSE_H_INCLUDED_
#define __HTTPRESPONSE_H_INCLUDED_

extern int sendHTTPNotFound(int sockfd);
extern char *generateHTMLFileList(char *directory);
extern int sendHTTPFileList(int sockfd, char *directory);

#endif

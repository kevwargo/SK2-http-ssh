#include <unistd.h>
#include <sys/types.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <libssh2.h>
#include <libssh2_sftp.h>

#include "socket-helpers.h"
#include "clientlist.h"
#include "httprequest.h"
#include "httpresponse.h"
#include "misc.h"


static char *GlobalPassword = NULL;
static pthread_mutex_t *PasswordMutex = NULL;

static long long sshSendLastError(long long (*sendfunc)(Client *client, char *msg),
                            Client *client, char *message)
{
    char msg[1024];
    char *errmsg = NULL;
    int errmsglen = 0;
    libssh2_session_last_error(client->ssh_session, &errmsg, &errmsglen, 0);
    sprintf(msg, "%s: %s", message, errmsg);
    return (*sendfunc)(client, msg);
}

static void kbd_emulate(const char *name, int name_len,
                        const char *instruction, int instruction_len, int num_prompts,
                        const LIBSSH2_USERAUTH_KBDINT_PROMPT *prompts,
                        LIBSSH2_USERAUTH_KBDINT_RESPONSE *responses,
                        void **abstract)
{
    for (int i = 0; i < num_prompts; i++)
    {
        responses[i].text = strdup(GlobalPassword);
        printf("sending password %s\n", GlobalPassword);
        responses[i].length = strlen(GlobalPassword);
    }
}

char *sshAllocHTMLFileListBuffer(LIBSSH2_SFTP *sftp_session, char *dirname,
                                 int *filecount)
{
    int size = strlen(HTMLFileListHeader);
    size += strlen(HTMLFileListTail);
    size += strlen(dirname);
    LIBSSH2_SFTP_HANDLE *dir = libssh2_sftp_opendir(sftp_session, dirname);
    if (! dir)
        return NULL;
    char filename[256];
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int namelen;
    int tmpllen = strlen(HTMLFileListEntryTemplate);
    *filecount = 0;
    while ((namelen = libssh2_sftp_readdir(dir, filename, 256, &attrs)) > 0)
    {
        if (strcmp(filename, ".") == 0)
            continue;
        size += tmpllen;
        size += namelen * 2;
        (*filecount)++;
    }
    libssh2_sftp_closedir(dir);
    if (namelen < 0)
        return NULL;
    return (char *)malloc(size);
}

int fnamecompar(void *arg1, void *arg2)
{
    char *name1 = *((char **)arg1);
    char *name2 = *((char **)arg2);
    return strcmp(name1, name2);
}

char *sshGenerateHTMLFileList(LIBSSH2_SESSION *ssh_session,
                              LIBSSH2_SFTP *sftp_session, char *dirname,
                              char *hostname)
{
    int filecount;
    char *htmlfilelist = sshAllocHTMLFileListBuffer(sftp_session, dirname,
                                                    &filecount); // free
    if (! htmlfilelist)
        return NULL;
    char **filenames = (char **)malloc(sizeof(char *) * filecount);
    LIBSSH2_SFTP_HANDLE *dir = libssh2_sftp_opendir(sftp_session, dirname);
    char name[256];
    int namelen;
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int realcount = 0;
    while ((namelen = libssh2_sftp_readdir(dir, name, 256, &attrs)) > 0)
    {
        if (! (LIBSSH2_SFTP_S_ISREG(attrs.permissions) ||
               LIBSSH2_SFTP_S_ISDIR(attrs.permissions) ||
               LIBSSH2_SFTP_S_ISLNK(attrs.permissions)))
            continue;
        if (realcount >= filecount)
        {
            printf("|||||||||||||||||||| %d %d realloc\n", filecount, realcount + 1);
            filenames = (char **)realloc(filenames, sizeof(char *) * (realcount + 1));
        }
        filenames[realcount] = (char *)malloc(namelen + 2 /*nul-byte and optional slash*/);
        strcpy(filenames[realcount], name);
        if (LIBSSH2_SFTP_S_ISDIR(attrs.permissions))
        {
            filenames[realcount][namelen] = '/';
            filenames[realcount][namelen + 1] = '\0';
        }
        realcount++;
    }
    printf("NAMELEN %d\n", namelen);
    if (namelen < 0)
    {
        int msglen = 0;
        char *errmsg = NULL;
        libssh2_session_last_error(ssh_session, &errmsg, &msglen, 0);
        printf("LIBSSH2 ERROR: %s\n", errmsg);
    }
    printf("realcount %s %d\n", dirname, realcount);
    qsort(filenames, realcount, sizeof(char *),
          (int(*)(const void *, const void *))fnamecompar);
    for (int i = 0; i < realcount; i++)
        printf("ssh file: %s\n", filenames[i]);
    int size = sprintf(htmlfilelist, HTMLFileListHeader, dirname, "");
    for (int i = 0; i < realcount; i++)
    {
        if (strcmp(filenames[i], "./") == 0)
            continue;
        if (strcmp(filenames[i], "../") == 0 &&
            strcmp(dirname, "/") == 0)
            continue;
        size += sprintf(htmlfilelist + size, HTMLFileListEntryTemplate,
                        filenames[i], "", filenames[i], "");
        free(filenames[i]);
    }
    free(filenames);
    strcpy(htmlfilelist + size, HTMLFileListTail);
    size += strlen(HTMLFileListTail);
    char *html_tmpl = (char *)malloc(strlen(HTMLTemplate) + strlen("Index of %s:%s")); //free
    size += sprintf(html_tmpl, HTMLTemplate, "Index of %s:%s", "%s");
    char *html = (char *)malloc(strlen(html_tmpl) + strlen(dirname) +
                                strlen(hostname) + strlen(htmlfilelist));
    int n = sprintf(html, html_tmpl, hostname, dirname, htmlfilelist);
    free(htmlfilelist);
    free(html_tmpl);
    return html;
}

long long sshSendHTTPFileList(Client *client, char *dirname, int withdata)
{
    long long result;
    char *html = sshGenerateHTMLFileList(client->ssh_session, client->sftp_session,
                                         dirname, client->ssh_host);
    if (! html)
        result = sendHTTPInternalServerError(client,
                                             "Cannot get directory file list");
    else
        result = sendHTTPFileList(client, html, withdata);
    free(html);
    return result;
}

static HTTPResponse *sshBuildFileResponse(LIBSSH2_SFTP_HANDLE *fd, char *buffer,
                                          int bufsize, int withdata)
{
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    if (libssh2_sftp_fstat(fd, &attrs) < 0)
        return NULL;
    HTTPResponse *response = buildDefaultResponse(200, "OK");
    setHTTPHeaderValue(&response->message, "Content-type", "application/octet-stream");
    if (withdata)
    {
        setHTTPHeaderNumberValue(&response->message, "Content-length", attrs.filesize);
        int bytes = libssh2_sftp_read(fd, buffer, bufsize);
        if (bytes < 0)
        {
            destroyResponse(response);
            return NULL;
        }
        response->message.datalen = bytes;
        setData(&response->message, buffer, bytes);
    }
    return response;
}

long long sshSendHTTPFile(Client *client, LIBSSH2_SFTP_HANDLE *fd, int withdata)
{
    long long total;
    int bufsize = 262144;
    char *buffer = (char *)malloc(bufsize);
    HTTPResponse *response = sshBuildFileResponse(fd, buffer, bufsize, withdata);
    if (! response)
        total = -2;
    else
    {
        long long bytes;
        int responselen;
        char *responsebuf = encodeResponse(response, &responselen);
        bytes = sendall(client->sockfd, responsebuf, responselen);
        free(responsebuf);
        destroyResponse(response);
        if (bytes < 0)
            total = -1;
        else
        {
            total += bytes;
            if (withdata)
            {
                while ((bytes = libssh2_sftp_read(fd, buffer, bufsize)) > 0)
                {
                    long long sent = sendall(client->sockfd, buffer, bytes);
                    if (sent < bytes)
                    {
                        free(buffer);
                        /* printf("tut\n"); */
                        return -1;
                    }
                    /* printf("sent chunk %d\n", sent); */
                    total += sent;
                }
                if (bytes < 0)
                    total = -2;
            }
        }
    }
    if (total > 0)
        logmsg(client, stdout, "Successfully sent %lld bytes from ssh\n", total);
    else
        logmsg(client, stdout, "Error while sending from ssh: %lld\n", total);
    free(buffer);
    return total;
}

long long getSSHResource(Client *client, HTTPRequest *request, char *sshresource)
{
    long long result;
    if (! client->sftp_session)
        return sendHTTPForbidden(client, "You are not logged in to a ssh-session");
    LIBSSH2_SFTP_HANDLE *dir = NULL;
    LIBSSH2_SFTP_HANDLE *file = NULL;
    dir = libssh2_sftp_opendir(client->sftp_session, sshresource);
    if (! dir)
    {
        file = libssh2_sftp_open(client->sftp_session, sshresource, LIBSSH2_FXF_READ, 0);
        if (! file)
            return sshSendLastError(sendHTTPInternalServerError, client,
                                    "Cannot open requested file or directory");
    }
    if (dir)
    {
        logmsg(client, stdout, "Sending dir %s from ssh\n", sshresource);
        libssh2_sftp_closedir(dir);
        result = sshSendHTTPFileList(client, sshresource,
                                     request->method == HTTP_GET_METHOD);
    }
    else
    {
        logmsg(client, stdout, "Sending file %s from ssh\n", sshresource);
        result = sshSendHTTPFile(client, file, request->method == HTTP_GET_METHOD);
        libssh2_sftp_close(file);
    }
    return result;
}

long long sshConnect(Client *client, char *host, char *username, char *password)
{
    long long result;
    struct addrinfo hints;
    struct addrinfo *ainfo, *rp;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    int res = getaddrinfo(host, "ssh", &hints, &ainfo);
    if (res != 0)
    {
        char *reason = (char *)malloc(1024);
        sprintf(reason, "Cannot find ssh server: %s", gai_strerror(res));
        result = sendHTTPBadGateway(client, reason);
        free(reason);
        return result;
    }
    int sockfd;
    for (rp = ainfo; rp; rp = rp->ai_next)
    {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd < 0)
            continue;
        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == 0)
            break;
        close(sockfd);
    }
    if (! rp)
    {
        char *reason = (char *)malloc(1024);
        sprintf(reason, "Cannot connect to ssh server: %s", gai_strerror(res));
        result = sendHTTPBadGateway(client, reason);
        free(reason);
        return result;
    }
    int rc;
    if ((rc = libssh2_init(0)) != 0)
    {
        logmsg(client, stderr, "libssh2 initialization failed %d\n", rc);
        close(sockfd);
        return sendHTTPInternalServerError(client, "libssh2 initialization failed");
    }
    client->ssh_session = libssh2_session_init();
    if (! client->ssh_session)
    {
        logmsg(client, stderr, "ssh session initialization failed\n");
        close(sockfd);
        return sendHTTPInternalServerError(client, "ssh session initialization failed");
    }
    if ((rc = libssh2_session_handshake(client->ssh_session, sockfd)) != 0)
    {
        logmsg(client, stderr, "Failure establishing SSH session: %d\n", rc);
        result = sendHTTPInternalServerError(client, "Failure establishing SSH session");
        goto error;
    }

    char *userauthlist = libssh2_userauth_list(client->ssh_session,
                                               username, strlen(username));
    fprintf(stderr, "Authentication methods: %s\n", userauthlist);
    if (strstr(userauthlist, "password") != NULL)
    {
        printf("!!!!! pwd auth is allowed\n");
        if (libssh2_userauth_password(client->ssh_session, username, password) != 0)
        {
            logmsg(client, stderr, "Authentication by password failed.\n");
            result = sendHTTPForbidden(client, "Authentication by password failed.");
            goto error;
        }
    }
    else if (strstr(userauthlist, "keyboard-interactive") != NULL)
    {
        printf("!!!!! kbd auth allowed\n");
        if (! PasswordMutex)
        {
            PasswordMutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
            pthread_mutex_init(PasswordMutex, NULL);
        }
        pthread_mutex_lock(PasswordMutex);
        GlobalPassword = password;
        if (libssh2_userauth_keyboard_interactive(client->ssh_session, username, &kbd_emulate) != 0)
        {
            logmsg(client, stderr,
                   "Authentication by keyboard-interactive failed!\n");
            result = sendHTTPForbidden(client,
                                       "Authentication by keyboard-interactive failed!");
            pthread_mutex_unlock(PasswordMutex);
            goto error;
        }
        pthread_mutex_unlock(PasswordMutex);
    }
    else
    {
        logmsg(client, stderr, "No supported authentication methods found!\n");
        result = sendHTTPInternalServerError(client,
                                             "No supported authentication methods found!");
        goto error;
    }

    logmsg(client, stdout, "SUCCESSFULLY CONNECTED AND AUTHENTICATED!!!!\n");

    client->sftp_session = libssh2_sftp_init(client->ssh_session);
    if (! client->sftp_session)
    {
        logmsg(client, stderr, "Unable to init SFTP session\n");
        result = sendHTTPInternalServerError(client, "Unable to init SFTP session");
        goto error;
    }

    logmsg(client, stdout, "SUCCESSFULLY CREATED SFTP SESSION!!!!\n");
    client->ssh_host = strdup(host);

    result = sendHTTPGeneral(client, 200, "OK", "Connected",
                             "<h1 align=center>Connected successfully :)</h1>"
                             "<h2 align=center>Now you can"
                             " <a href=/ssh/>Enter ssh root dir</a></h2>");
    
    return result;

error:
    close(sockfd);
    libssh2_session_disconnect(client->ssh_session, "Disconnecting");
    libssh2_session_free(client->ssh_session);
    client->ssh_session = NULL;
    
    return result;
}

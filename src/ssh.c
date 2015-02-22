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


static void kbd_callback(const char *name, int name_len,
                         const char *instruction, int instruction_len, int num_prompts,
                         const LIBSSH2_USERAUTH_KBDINT_PROMPT *prompts,
                         LIBSSH2_USERAUTH_KBDINT_RESPONSE *responses,
                         void **abstract)
{
    int i;
    size_t n;
    char buf[1024];
    (void)abstract;
 
    fprintf(stderr, "Performing keyboard-interactive authentication.\n");
 
    fprintf(stderr, "Authentication name: '");
    fwrite(name, 1, name_len, stderr);
    fprintf(stderr, "'\n");
 
    fprintf(stderr, "Authentication instruction: '");
    fwrite(instruction, 1, instruction_len, stderr);
    fprintf(stderr, "'\n");
 
    fprintf(stderr, "Number of prompts: %d\n\n", num_prompts);
 
    for (i = 0; i < num_prompts; i++) {
        fprintf(stderr, "Prompt %d from server: '", i);
        fwrite(prompts[i].text, 1, prompts[i].length, stderr);
        fprintf(stderr, "'\n");
 
        fprintf(stderr, "Please type response: ");
        char *r = fgets(buf, sizeof(buf), stdin);
        n = strlen(buf);
        while (n > 0 && strchr("\r\n", buf[n - 1]))
            n--;
        buf[n] = 0;
 
        responses[i].text = strdup(buf);
        responses[i].length = n;
 
        fprintf(stderr, "Response %d from user is '", i);
        fwrite(responses[i].text, 1, responses[i].length, stderr);
        fprintf(stderr, "'\n\n");
    }
 
    fprintf(stderr,
            "Done. Sending keyboard-interactive responses to server now.\n");
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


int ssh_test(int argc, char **argv)
{
    const char *keyfile1="~/.ssh/id_rsa.pub";
    const char *keyfile2="~/.ssh/id_rsa";
    const char *username="username";
    const char *password="password";
    const char *sftppath="/tmp/TEST";

    
    unsigned long hostaddr;
    int sock, i, auth_pw = 0;
    struct sockaddr_in sin;
    const char *fingerprint;
    char *userauthlist;
    LIBSSH2_SESSION *session;
    int rc;
    LIBSSH2_SFTP *sftp_session;
    LIBSSH2_SFTP_HANDLE *sftp_handle;

    if (argc < 4)
    {
        fprintf(stderr, "usage: %s HOST USERNAME PASSWORD\n", argv[0]);
        return 1;
    }

    username = argv[2];

    rc = libssh2_init (0);

    if (rc != 0) {
        fprintf(stderr, "libssh2 initialization failed (%d)\n", rc);
        return 1;
    }

    /*
     * The application code is responsible for creating the socket
     * and establishing the connection
     */
    sock = socket(AF_INET, SOCK_STREAM, 0);

    sin.sin_family = AF_INET;
    sin.sin_port = htons(22);
    if (! inet_aton(argv[1], &(sin.sin_addr)))
    {
        fprintf(stderr, "wrong address %s\n", argv[1]);
        return 1;
    }
    if (connect(sock, (struct sockaddr*)(&sin),
                sizeof(struct sockaddr_in)) != 0) {
        fprintf(stderr, "failed to connect!\n");
        return -1;
    }

    /* Create a session instance
     */
    session = libssh2_session_init();

    if(!session)
        return -1;

    /* Since we have set non-blocking, tell libssh2 we are blocking */
    libssh2_session_set_blocking(session, 1);


    /* ... start it up. This will trade welcome banners, exchange keys,
     * and setup crypto, compression, and MAC layers
     */
    rc = libssh2_session_handshake(session, sock);

    if(rc) {
        fprintf(stderr, "Failure establishing SSH session: %d\n", rc);
        return -1;
    }

    /* At this point we havn't yet authenticated.  The first thing to do
     * is check the hostkey's fingerprint against our known hosts Your app
     * may have it hard coded, may go to a file, may present it to the
     * user, that's your call
     */
    fingerprint = libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_SHA1);

    fprintf(stderr, "Fingerprint: ");
    for(i = 0; i < 20; i++) {
        fprintf(stderr, "%02X ", (unsigned char)fingerprint[i]);
    }
    fprintf(stderr, "\n");

    /* check what authentication methods are available */
    userauthlist = libssh2_userauth_list(session, username, strlen(username));

    fprintf(stderr, "Authentication methods: %s\n", userauthlist);
    if (strstr(userauthlist, "password") != NULL) {
        auth_pw |= 1;
        printf("!!!!! pwd auth is allowed\n");
    }
    if (strstr(userauthlist, "keyboard-interactive") != NULL) {
        auth_pw |= 2;
        printf("!!!!! kbd auth allowed\n");
    }
    if (strstr(userauthlist, "publickey") != NULL) {
        auth_pw |= 4;
        printf("!!!!! pubkey allowed\n");
    }

    /* if we got an 4. argument we set this option if supported */
    if(argc > 5) {
        if ((auth_pw & 1) && !strcasecmp(argv[5], "-p")) {
            auth_pw = 1;
        }
        if ((auth_pw & 2) && !strcasecmp(argv[5], "-i")) {
            auth_pw = 2;
        }
        if ((auth_pw & 4) && !strcasecmp(argv[5], "-k")) {
            auth_pw = 4;
        }
    }

    if (auth_pw & 1) {
        /* We could authenticate via password */
        if (libssh2_userauth_password(session, username, password)) {

            fprintf(stderr, "Authentication by password failed.\n");
            goto shutdown;
        }
    } else if (auth_pw & 2) {
        /* Or via keyboard-interactive */
        if (libssh2_userauth_keyboard_interactive(session, username, &kbd_callback) ) {

            fprintf(stderr,
                    "\tAuthentication by keyboard-interactive failed!\n");
            goto shutdown;
        } else {
            fprintf(stderr,
                    "\tAuthentication by keyboard-interactive succeeded.\n");
        }
    } else if (auth_pw & 4) {
        /* Or by public key */
        if (libssh2_userauth_publickey_fromfile(session, username, keyfile1, keyfile2, password)) {

            fprintf(stderr, "\tAuthentication by public key failed!\n");
            goto shutdown;
        } else {
            fprintf(stderr, "\tAuthentication by public key succeeded.\n");
        }
    } else {
        fprintf(stderr, "No supported authentication methods found!\n");
        goto shutdown;
    }

    printf("SUCCESSFULLY CONNECTED!!!!!!!!!!!!!!!\n");
    goto shutdown;

    fprintf(stderr, "libssh2_sftp_init()!\n");

    sftp_session = libssh2_sftp_init(session);


    if (!sftp_session) {
        fprintf(stderr, "Unable to init SFTP session\n");
        goto shutdown;
    }

    fprintf(stderr, "libssh2_sftp_open()!\n");

    /* Request a file via SFTP */
    sftp_handle =
        libssh2_sftp_open(sftp_session, sftppath, LIBSSH2_FXF_READ, 0);


    if (!sftp_handle) {
        fprintf(stderr, "Unable to open file with SFTP: %ld\n",
                libssh2_sftp_last_error(sftp_session));

        goto shutdown;
    }
    fprintf(stderr, "libssh2_sftp_open() is done, now receive data!\n");

    do {
        char mem[1024];

        /* loop until we fail */
        fprintf(stderr, "libssh2_sftp_read()!\n");

        rc = libssh2_sftp_read(sftp_handle, mem, sizeof(mem));

        if (rc > 0) {
            int w = write(1, mem, rc);
        } else {
            break;
        }
    } while (1);

    libssh2_sftp_close(sftp_handle);

    libssh2_sftp_shutdown(sftp_session);


shutdown:

    libssh2_session_disconnect(session, "Normal Shutdown, Thank you for playing");

    libssh2_session_free(session);


    close(sock);
    fprintf(stderr, "all done\n");

    libssh2_exit();


    return 0;
}


int getSSHResource(Client *client, HTTPRequest *request, char *sshrequest)
{
    return 1;
}

int sshConnect(Client *client, char *host, char *username, char *password)
{
    int result;
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
        goto shutdown;
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
            goto shutdown;
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
            goto shutdown;
        }
        pthread_mutex_unlock(PasswordMutex);
    }
    else
    {
        logmsg(client, stderr, "No supported authentication methods found!\n");
        result = sendHTTPInternalServerError(client,
                                             "No supported authentication methods found!");
        goto shutdown;
    }

    logmsg(client, stdout, "SUCCESSFULLY CONNECTED AND AUTHENTICATED!!!!\n");
    
shutdown:
    close(sockfd);
    libssh2_session_disconnect(client->ssh_session, "Disconnecting");
    libssh2_session_free(client->ssh_session);
    client->ssh_session = NULL;
    return result;
}

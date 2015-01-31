#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include "socket-helpers.h"
#include "misc.h"


int sendHTTPNotFound(int sockfd)
{
    int bufsize = 1024;
    char *buffer = (char *)malloc(bufsize);
    char *date = curdate();
    char *html = (char *)malloc(896);
    sprintf(html,
            "<html>\n"
            "<head><title>LEPECBEKE</title></head>\n"
            "<body>\n"
            "<hr/>\n"
            "<h1>IT'S WORKING!!!!!!!!!!!!!!!! :D</h1>\n"
            "<h2>Current GMT date: %s</h2>\n"
            "<hr/>\n"
            "</body>\n"
            "</html>\n", date);
    sprintf(buffer,
            "HTTP/1.1 404 Not Found\r\n"
            "Server: lepecbeke\r\n"
            "Date: %s\r\n"
            "Content-type: text/html\r\n"
            "Content-length: %d\r\n"
            "\r\n"
            "%s", date, (int)strlen(html), html);

    int sent = sendall(sockfd, buffer, strlen(buffer));
    free(buffer);
    free(html);
    return sent;
}

/* THE RETURN VALUE MUST BE FREED !!! */
char *generateHTMLFileList(char *directory)
{
    int bufsize = 1024;
    char *html = (char *)malloc(bufsize);
    sprintf(html,
            "<html>\n"
            "<head><title>Index of %s/</title></head>\n"
            "<body>\n"
            "<h1>Index of %s/</h1>\n"
            "<hr/>\n"
            "<pre>\n", directory, directory);
    int occupied = strlen(html);
    DIR *dir = opendir(directory);
    if (! dir)
    {
        fprintf(stderr, "Cannot open %s: %s\n", directory, strerror(errno));
        free(html);
        return NULL;
    }
    int html_entry_size = strlen("<a href=\"\"></a>\n");
    char *html_tail = "</pre>\n<hr/>\n</body>\n</html>\n";
    struct dirent *entry;
    while (entry = readdir(dir))
    {
        while (bufsize - occupied < strlen(entry->d_name)*2 + html_entry_size + 2)
        {
            bufsize += 512;
            html = realloc(html, bufsize);
        }
        if ((entry->d_type == DT_DIR || entry->d_type == DT_REG) &&
            strcmp(entry->d_name, ".") != 0)
        {
            sprintf(html + occupied,
                    "<a href=\"%s%s\">%s%s</a>\n",
                    entry->d_name, entry->d_type == DT_DIR ? "/" : "",
                    entry->d_name, entry->d_type == DT_DIR ? "/" : "");
            occupied += strlen(html + occupied);
        }
    }
    if (bufsize - occupied < strlen(html_tail))
    {
        bufsize += strlen(html_tail);
        html = realloc(html, bufsize);
    }
    sprintf(html + occupied, "%s", html_tail);
    return html;
}

int sendHTTPFileList(int sockfd, char *directory)
{
    int bufsize = 1024;
    char *buffer = (char *)malloc(bufsize);
    char *html = generateHTMLFileList(directory);
    if (! html)
        return -1;
    int html_len = strlen(html);
    sprintf(buffer,
            "HTTP/1.1 200 OK\r\n"
            "Server: lepecbeke\r\n"
            "Date: %s\r\n"
            "Content-type: text/html\r\n"
            "Content-length: %d\r\n"
            "\r\n", curdate(), html_len);
    int status = sendall(sockfd, buffer, strlen(buffer));
    free(buffer);
    if (status < 0)
    {
        free(html);
        return status;
    }
    status = sendall(sockfd, html, html_len);
    free(html);
    return status;
}


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>

char *getMimeType(char *filename)
{
    int fifo[2];
    if (pipe(fifo) < 0)
        return NULL;
    if (fork())
    {
        close(fifo[1]);
        FILE *input = fdopen(fifo[0], "r");
        size_t size = 0;
        char *buffer = (char *)malloc(1024);
        if (getline(&buffer, &size, input) < 0 && errno != 0)
            return NULL;
        int n = strlen(buffer);
        while (strchr("\r\n", buffer[n - 1]))
        {
            buffer[n - 1] = '\0';
            n--;
        }
        wait(NULL);
        fclose(input);
        return buffer;
    }
    else
    {
        dup2(fifo[1], 1);
        execlp("file", "file", "-b", "--mime-type", filename, NULL);
    }
}

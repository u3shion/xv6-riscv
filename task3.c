#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#define BUFF_SIZE 4096

int main(int argc, char *argv[])
{
    int pipefd[2];
    pid_t pid;
    char buffer[BUFF_SIZE];
    int n;

    if (pipe(pipefd) == -1)
    {
        perror("pipe");
        exit(1);
    }

    pid = fork();
    if (pid == -1)
    {
        perror("fork");
        exit(1);
    }

    if (pid == 0)
    {
        close(pipefd[1]);

        while ((n = read(pipefd[0], buffer, BUFF_SIZE)) > 0)
        {
            if (write(1, buffer, n) != n)
            {
                perror("write to stdout");
                exit(1);
            }
        }

        if (n == -1)
        {
            perror("read from pipe");
            exit(1);
        }

        close(pipefd[0]);
        exit(0);
    }
    else
    {
        close(pipefd[0]);

        char buf[BUFF_SIZE];
        int ofs = 0;

        for (int i = 1; i < argc; i++)
        {
            char *arg = argv[i];
            int len = strlen(arg);

            if (ofs + len + 1 > BUFF_SIZE)
            {
                if (write(pipefd[1], buf, ofs) != ofs)
                {
                    perror("write to pipe");
                    exit(1);
                }

                ofs = 0;
            }

            memcpy(buf + ofs, arg, len);
            ofs += len;
            buf[ofs] = '\n';
            ofs += 1;
        }

        if (ofs > 0)
        {
            if (write(pipefd[1], buf, ofs) != ofs)
            {
                perror("write to pipe");
                exit(1);
            }
        }

        close(pipefd[1]);
        wait(NULL);
        exit(0);
    }
}
#include "kernel/types.h"
#include "user/user.h"

#define BUFF_SIZE 512

int main(int argc, char *argv[])
{
    int pipefd[2];
    int pid;
    char buf[BUFF_SIZE];

    if (pipe(pipefd) < 0)
    {
        fprintf(2, "pipe error\n");
        exit(1);
    }

    pid = fork();
    if (pid < 0)
    {
        fprintf(2, "fork error\n");
        exit(1);
    }

    if (pid == 0)
    {
        close(pipefd[1]);

        close(0);
        dup(pipefd[0]);
        close(pipefd[0]);

        char *wc_argv[] = {"wc", 0};
        exec("/wc", wc_argv);

        fprintf(2, "exec wc error\n");
        exit(1);
    }
    else
    {
        close(pipefd[0]);

        int tot_b = 0, ofs = 0;

        for (int i = 1; i < argc; i++)
        {
            char *arg = argv[i];
            int len = strlen(arg);

            if (ofs + len + 1 > BUFF_SIZE)
            {
                if (write(pipefd[1], buf, ofs) != ofs)
                {
                    fprintf(2, "write error\n");
                    exit(1);
                }

                tot_b += ofs;
                ofs = 0;
            }

            memcpy(buf + ofs, arg, len);
            ofs += len;

            buf[ofs] = '\n';
            ofs++;
        }

        if (ofs > 0)
        {
            if (write(pipefd[1], buf, ofs) != ofs)
            {
                fprintf(2, "write error\n");
                exit(1);
            }

            tot_b += ofs;
        }

        close(pipefd[1]);
        wait(0);
        exit(0);
    }
}
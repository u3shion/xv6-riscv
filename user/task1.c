#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int pid, stat;
    int mode = 0;

    if (argc > 1 && argv[1][0] == 'k')
        mode = 1;

    pid = fork();

    if (pid < 0)
    {
        fprintf(2, "fork error\n");
        exit(1);
    }

    if (pid == 0)
    {
        pause(30);
        exit(1);
    }
    else
    {
        printf("%d %d\n", getpid(), pid);

        if (mode == 1)
        {
            kill(pid);
        }

        int wpid = wait(&stat);
        if (wpid < 0)
        {
            fprintf(2, "wait error\n");
            exit(1);
        }
        if (wpid != pid)
        {
            fprintf(2, "wait: expected pid %d, got %d\n", pid, wpid);
            exit(1);
        }
        printf("%d %d\n", wpid, stat);
        exit(0);
    }
}
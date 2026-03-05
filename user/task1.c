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

        wait(&stat);
        printf("%d %d\n", pid, stat);
        exit(0);
    }
}
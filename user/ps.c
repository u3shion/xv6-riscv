#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/procinfo.h"

#define INIT_SZ 8
#define MAX_BUF_SZ 256

const char *get_state_str(int state)
{
    switch (state)
    {
    case 0:
        return "UNUSED";
    case 1:
        return "USED";
    case 2:
        return "SLEEPING";
    case 3:
        return "RUNNABLE";
    case 4:
        return "RUNNING";
    case 5:
        return "ZOMBIE";
    default:
        return "UNKNOWN";
    }
}

void print_table_header()
{
    printf("\n");
    printf("  PID    NAME             STATE      PPID PNAME\n");
    printf("  -----------------------------------------------\n");
}

void print_proc(struct procinfo *p)
{
    printf("  %d ", p->pid);

    if (p->pid >= 10)
        printf("    %s", p->name);
    else
        printf("     %s", p->name);

    for (int i = strlen(p->name); i < 16; i++)
        printf(" ");
    printf(" ");

    const char *state = get_state_str(p->state);
    printf("%s", state);
    for (int i = strlen(state); i < 10; i++)
        printf(" ");
    printf(" ");

    printf("%d    %s\n", p->ppid, p->pname);
}

int main(int argc, char *argv[])
{
    struct procinfo *info = 0;
    int cnt;
    int buf_size = INIT_SZ;

    if (argc > 1)
    {
        int size = atoi(argv[1]);
        if (size > 0 && size < MAX_BUF_SZ)
        {
            buf_size = size;
        }
    }

    printf("ps: using initial buffer size %d\n", buf_size);

    while (1)
    {
        info = malloc(buf_size * sizeof(struct procinfo));
        if (info == 0)
        {
            fprintf(2, "ps: failed to allocate buffer of size %d\n", buf_size);
            exit(1);
        }

        cnt = ps_listinfo(info, buf_size);

        if (cnt >= 0)
            break;

        if (cnt == -1)
        {
            printf("ps: buffer too small (%d), increasing to %d\n", buf_size, buf_size * 2);

            free(info);
            buf_size *= 2;

            if (buf_size > MAX_BUF_SZ)
            {
                fprintf(2, "ps: buffer size limit reached (%d)\n", MAX_BUF_SZ);
                exit(1);
            }
        }
        else
        {
            fprintf(2, "ps: system call error %d\n", cnt);
            free(info);
            exit(1);
        }
    }

    for (int i = 0; i < cnt - 1; i++)
    {
        for (int j = 0; j < cnt - i - 1; j++)
        {
            if (info[j].pid > info[j + 1].pid)
            {
                struct procinfo temp = info[j];
                info[j] = info[j + 1];
                info[j + 1] = temp;
            }
        }
    }

    print_table_header();

    for (int i = 0; i < cnt; i++)
        print_proc(&info[i]);

    printf("\n");
    printf("  Total processes: %d (buffer used: %d/%d)\n", cnt, cnt, buf_size);

    free(info);
    exit(0);
}
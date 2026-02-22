#include "kernel/types.h"
#include "user/user.h"

#define BSIZE 64

void read_srt(char *buf)
{
    char *p = buf;
    int code;
    char s;

    while (p < buf + BSIZE - 1)
    {
        code = read(0, &s, 1);

        if (code < 0)
        {
            printf("%s", "Error in reading\n");
            exit(1);
        }

        if (code == 0)
            break;
        if (s == '\n')
            break;

        *p++ = s;
    }
    *p = '\0';
}

void parse_nums(char *buf, int *num1, int *num2)
{
    char *sp = buf;
    while (*sp != ' ' && *sp != '\0')
        sp++;

    if (sp == buf)
    {
        printf("%s", "Error: first symbol is space.\n");
        exit(1);
    }

    if (*sp != ' ')
    {
        printf("%s", "Error: no space after first number.\n");
        exit(1);
    }

    *sp = '\0';
    char *prt1 = buf;
    char *ptr2 = sp + 1;

    if (*ptr2 == '\0' || *ptr2 == ' ')
    {
        printf("%s", "Error: double space or no second number.\n");
        exit(1);
    }

    *num1 = atoi(prt1);
    *num2 = atoi(ptr2);
}

int main()
{
    char buf[BSIZE];

    read_srt(buf);
    printf("|%s|\n", buf);

    int num1, num2;
    parse_nums(buf, &num1, &num2);
    int sum = add(num1, num2);
    printf("%d + %d = %d\n", num1, num2, sum);
    return 0;
}
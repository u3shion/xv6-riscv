#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "syscall.h"

int sys_add(void)
{
    int a, b;
    argint(0, &a);
    argint(1, &b);
    return a + b;
}
#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

#define BUF_SIZE (DMESG_PAGES * 4096)

static char buf[BUF_SIZE];

int
main(void)
{
  int n = dmesg(buf, BUF_SIZE);

  if (n < 0) {
    fprintf(2, "dmesg: syscall failed\n");
    exit(1);
  }
  
  printf("%s", buf);
  exit(0);
}

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"

int mutexalloc(struct file **f) {
  struct sleeplock *m;

  *f = 0;
  m = 0;

  if ((*f = filealloc()) == 0)
    goto bad;

  if ((m = (struct sleeplock *)kalloc()) == 0)
    goto bad;

  initsleeplock(m, "mutex");
  printf("[mutexalloc] pid=%d mutex=%p\n", myproc()->pid, m);

  (*f)->type = FD_MUTEX;
  (*f)->readable = 0;
  (*f)->writable = 0;
  (*f)->mutex = m;
  return 0;

bad:
  printf("[mutexalloc] pid=%d failed\n", myproc()->pid);
  
  if (m)
    kfree((char *)m);
  if (*f)
    fileclose(*f);
  return -1;
}

void
mutexclose(struct sleeplock *m)
{
  printf("[mutexclose] pid=%d mutex=%p\n", myproc()->pid, m);
  kfree((char *)m);
}

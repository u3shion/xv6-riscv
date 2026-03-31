#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "file.h"
#include "proc.h"

static struct spinlock pseudo_lock;
static uint64 urandom_seed = 1;
static uint64 nullstat_written = 0;

static uint64
lcg_next(uint64 x)
{
  return x * 6364136223846793005ULL + 1;
}

int pseudoread(int minor, int user_dst, uint64 dst, int n)
{
  char buf[64];
  uint64 tmp;
  int i;

  if (minor == PSEUDO_NULL)
    return 0;

  if (minor == PSEUDO_ZERO)
  {
    i = 0;

    while (i < n)
    {
      int rem = n - i;

      if (rem > (int)sizeof(buf))
        rem = sizeof(buf);

      memset(buf, 0, rem);

      if (either_copyout(user_dst, dst + i, buf, rem) < 0)
        return -1;

      i += rem;
    }

    return n;
  }

  if (minor == PSEUDO_URANDOM)
  {
    i = 0;

    while (i < n)
    {
      int rem = n - i;

      if (rem > (int)sizeof(buf))
        rem = sizeof(buf);

      acquire(&pseudo_lock);
      for (int j = 0; j < rem; j++)
      {
        urandom_seed = lcg_next(urandom_seed);
        buf[j] = (char)(urandom_seed >> 56);
      }
      release(&pseudo_lock);

      if (either_copyout(user_dst, dst + i, buf, rem) < 0)
        return -1;

      i += rem;
    }

    return n;
  }

  if (minor == PSEUDO_NULLSTAT)
  {
    if (n != (int)sizeof(uint64))
      return -1;

    acquire(&pseudo_lock);
    tmp = nullstat_written;
    release(&pseudo_lock);

    if (either_copyout(user_dst, dst, &tmp, sizeof(tmp)) < 0)
      return -1;

    return sizeof(tmp);
  }

  return -1;
}

int pseudowrite(int minor, int user_src, uint64 src, int n)
{
  uint64 tmp;

  if (minor == PSEUDO_NULL)
    return n;

  if (minor == PSEUDO_ZERO)
    return -1;

  if (minor == PSEUDO_URANDOM)
  {
    if (n != (int)sizeof(uint64))
      return -1;

    if (either_copyin(&tmp, user_src, src, sizeof(tmp)) < 0)
      return -1;

    acquire(&pseudo_lock);
    urandom_seed = tmp;
    release(&pseudo_lock);

    return n;
  }

  if (minor == PSEUDO_NULLSTAT)
  {
    acquire(&pseudo_lock);
    nullstat_written += (uint64)n;
    release(&pseudo_lock);

    return n;
  }

  return -1;
}

void pseudoinit(void)
{
  initlock(&pseudo_lock, "pseudo");
  devsw[PSEUDO_MAJOR].read = pseudoread;
  devsw[PSEUDO_MAJOR].write = pseudowrite;
}

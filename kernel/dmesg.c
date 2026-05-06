#include <stdarg.h>

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

#define DMESG_SIZE (DMESG_PAGES * PGSIZE)

static char dmesg_buf[DMESG_SIZE];
static int  dmesg_head;
static int  dmesg_tail;
static struct spinlock dmesg_lock;

static struct spinlock log_lock;
int  log_flags   = 0;
static uint log_expire = 0;

static void
buf_putc(char c)
{
  int next = (dmesg_tail + 1) % DMESG_SIZE;
  
  if (next == dmesg_head)
    dmesg_head = (dmesg_head + 1) % DMESG_SIZE;

  dmesg_buf[dmesg_tail] = c;
  dmesg_tail = (dmesg_tail + 1) % DMESG_SIZE;
}

static void
buf_putstr(const char *s)
{
  while (*s)
    buf_putc(*s++);
}

static void
buf_putint(long long xx, int base, int sign)
{
  static char digits[] = "0123456789abcdef";
  char tmp[20];
  int i = 0;
  unsigned long long x;

  if (sign && xx < 0) {
    buf_putc('-');
    x = (unsigned long long)(-xx);
  } else {
    x = (unsigned long long)xx;
  }

  do {
    tmp[i++] = digits[x % base];
  } while ((x /= base) != 0);

  while (--i >= 0)
    buf_putc(tmp[i]);
}

static void
buf_putuint(unsigned long long x, int base)
{
  static char digits[] = "0123456789abcdef";
  char tmp[20];
  int i = 0;

  do {
    tmp[i++] = digits[x % base];
  } while ((x /= base) != 0);

  while (--i >= 0)
    buf_putc(tmp[i]);
}

void
dmesg_init(void)
{
  initlock(&dmesg_lock, "dmesg");
  initlock(&log_lock, "logctl");
  dmesg_head = 0;
  dmesg_tail = 0;
  acquire(&dmesg_lock);
  buf_putc('\n');
  release(&dmesg_lock);
}

void
pr_msg(const char *fmt, ...)
{
  va_list ap;
  int i, cx, c0, c1, c2;
  char *s;
  uint t;

  acquire(&tickslock);
  t = ticks;
  release(&tickslock);

  acquire(&dmesg_lock);

  buf_putc('[');
  buf_putuint(t, 10);
  buf_putc(']');
  buf_putc(' ');

  va_start(ap, fmt);
  for (i = 0; (cx = fmt[i] & 0xff) != 0; i++) {
    if (cx != '%') {
      buf_putc(cx);
      continue;
    }

    i++;
    c0 = fmt[i+0] & 0xff;
    c1 = c2 = 0;
    if (c0) c1 = fmt[i+1] & 0xff;
    if (c1) c2 = fmt[i+2] & 0xff;

    if (c0 == 'd') {
      buf_putint(va_arg(ap, int), 10, 1);
    } else if (c0 == 'l' && c1 == 'd') {
      buf_putint(va_arg(ap, long), 10, 1);
      i += 1;
    } else if (c0 == 'l' && c1 == 'l' && c2 == 'd') {
      buf_putint(va_arg(ap, long long), 10, 1);
      i += 2;
    } else if (c0 == 'u') {
      buf_putuint(va_arg(ap, unsigned int), 10);
    } else if (c0 == 'l' && c1 == 'u') {
      buf_putuint(va_arg(ap, unsigned long), 10);
      i += 1;
    } else if (c0 == 'l' && c1 == 'l' && c2 == 'u') {
      buf_putuint(va_arg(ap, unsigned long long), 10);
      i += 2;
    } else if (c0 == 'x') {
      buf_putuint(va_arg(ap, unsigned int), 16);
    } else if (c0 == 'l' && c1 == 'x') {
      buf_putuint(va_arg(ap, unsigned long), 16);
      i += 1;
    } else if (c0 == 'l' && c1 == 'l' && c2 == 'x') {
      buf_putuint(va_arg(ap, unsigned long long), 16);
      i += 2;
    } else if (c0 == 'p') {
      buf_putc('0'); buf_putc('x');
      buf_putuint(va_arg(ap, uint64), 16);
    } else if (c0 == 'c') {
      buf_putc((char)va_arg(ap, int));
    } else if (c0 == 's') {
      s = va_arg(ap, char *);
      if (s == 0) s = "(null)";
      buf_putstr(s);
    } else if (c0 == '%') {
      buf_putc('%');
    } else if (c0 == 0) {
      break;
    } else {
      buf_putc('%');
      buf_putc(c0);
    }
  }
  va_end(ap);

  buf_putc('\n');

  release(&dmesg_lock);
}

int
log_active(int flag)
{
  int active;
  uint t, exp;

  acquire(&log_lock);
  if (!(log_flags & flag)) {
    release(&log_lock);
    return 0;
  }
  exp = log_expire;
  release(&log_lock);

  if (exp == 0)
    return 1;

  acquire(&tickslock);
  t = ticks;
  release(&tickslock);

  active = (t < exp);
  if (!active) {
    acquire(&log_lock);
    log_expire = 0;
    log_flags  = 0;
    release(&log_lock);
  }
  return active;
}

uint64
sys_dmesg(void)
{
  uint64 ubuf;
  int    maxlen;
  argaddr(0, &ubuf);
  argint(1, &maxlen);

  if (maxlen <= 0)
    return -1;

  struct proc *p = myproc();

  acquire(&dmesg_lock);

  int head = dmesg_head;
  int tail = dmesg_tail;

  int start = head;
  int len   = (tail - head + DMESG_SIZE) % DMESG_SIZE;
  int found = 0;
  for (int i = 0; i < len; i++) {
    int idx = (head + i) % DMESG_SIZE;

    if (dmesg_buf[idx] == '\n') {
      start = (idx + 1) % DMESG_SIZE;
      found = 1;
      break;
    }
  }
  (void)found;

  int avail = (tail - start + DMESG_SIZE) % DMESG_SIZE;
  int copy  = avail;
  if (copy > maxlen - 1)
    copy = maxlen - 1;

  release(&dmesg_lock);

  int written = 0;
  if (copy > 0) {
    int first = (DMESG_SIZE - start);

    if (first > copy) first = copy;

    if (copyout(p->pagetable, ubuf + written, dmesg_buf + start, first) < 0)
      return -1;

    written += first;

    int second = copy - first;
    if (second > 0) {
      if (copyout(p->pagetable, ubuf + written, dmesg_buf, second) < 0)
        return -1;

      written += second;
    }
  }

  char nul = 0;
  if (copyout(p->pagetable, ubuf + written, &nul, 1) < 0)
    return -1;

  return written;
}

uint64
sys_logctl(void)
{
  int flags, duration;
  argint(0, &flags);
  argint(1, &duration);

  acquire(&log_lock);
  log_flags = flags;
  
  if (flags == 0) {
    log_expire = 0;
  } else if (duration > 0) {
    acquire(&tickslock);
    log_expire = ticks + (uint)duration;
    release(&tickslock);
  } else {
    log_expire = 0;
  }
  release(&log_lock);

  return 0;
}

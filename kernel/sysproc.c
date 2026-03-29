#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"
#include "procinfo.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0; // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if (t == SBRK_EAGER || n < 0)
  {
    if (growproc(n) < 0)
    {
      return -1;
    }
  }
  else
  {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if (addr + n < addr)
      return -1;
    if (addr + n > TRAPFRAME)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}

uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if (n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (killed(myproc()))
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kkill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_ps_listinfo(void)
{
  uint64 plist_addr;
  int lim;
  struct procinfo plist;
  struct proc *p;
  int cnt = 0;

  argaddr(0, &plist_addr);
  argint(1, &lim);

  if (plist_addr == 0)
  {
    acquire(&wait_lock);
    for (p = proc; p < &proc[NPROC]; p++)
    {
      acquire(&p->lock);
      if (p->state != UNUSED)
        cnt++;
      release(&p->lock);
    }
    release(&wait_lock);
    return cnt;
  }

  if (lim <= 0)
    return -2;

  acquire(&wait_lock);
  for (p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    if (p->state != UNUSED)
    {
      plist.pid = p->pid;
      safestrcpy(plist.name, p->name, sizeof(plist.name));
      plist.state = p->state;

      if (p->parent != 0)
      {
        plist.ppid = p->parent->pid;
        safestrcpy(plist.pname, p->parent->name, sizeof(plist.pname));
      }
      else
      {
        plist.ppid = -1;
        plist.pname[0] = '\0';
      }

      if (cnt >= lim)
      {
        release(&p->lock);
        release(&wait_lock);
        return -1;
      }

      if ((uint64)cnt > ((~(uint64)0) - plist_addr) / sizeof(struct procinfo)) {
        release(&p->lock);
        release(&wait_lock);
        return -3;
      }
      uint64 user_addr = plist_addr + (uint64)cnt * sizeof(struct procinfo);

      if (copyout(myproc()->pagetable, user_addr, (char *)&plist, sizeof(plist)) < 0)
      {
        release(&p->lock);
        release(&wait_lock);
        return -3;
      }

      cnt++;
    }

    release(&p->lock);
  }

  release(&wait_lock);
  return cnt;
}

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"

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

void printwalk(pagetable_t pagetable, int lev, uint64 vpn_pref)
{
  for (int i = 0; i < 512; i++)
  {
    pte_t pte = pagetable[i];

    if (pte & PTE_V)
    {
      for (int j = 0; j < lev; j++)
        printf(".. ");

      uint64 child = PTE2PA(pte);

      if ((pte & (PTE_R | PTE_W | PTE_X)) == 0)
      {
        printf("%d: pte %p pa %p\n", i, (void *)pte, (void *)child);
        printwalk((pagetable_t)child, lev + 1, (vpn_pref << 9) | i);
      }
      else
      {
        uint64 phys_addr = child;
        uint64 full_vpn = (vpn_pref << 9) | i;

        printf("%p -> %p ", (void *)full_vpn, (void *)phys_addr);

        printf("%c", (pte & PTE_R) ? 'R' : '_');
        printf("%c", (pte & PTE_W) ? 'W' : '_');
        printf("%c", (pte & PTE_X) ? 'X' : '_');
        printf("%c", (pte & PTE_U) ? 'U' : '_');
        printf("%c", (pte & PTE_G) ? 'G' : '_');
        printf("%c", (pte & PTE_A) ? 'A' : '_');
        printf("%c", (pte & PTE_D) ? 'D' : '_');
        printf("\n");
      }
    }
  }
}

uint64
sys_printpgtable(void)
{
  struct proc *p = myproc();
  printf("PAGETABLE %p\n", (void *)p->pagetable);
  printwalk(p->pagetable, 0, 0);
  return 0;
}

uint64
sys_clearflags(void)
{
  uint64 buf;
  int len, mask;
  struct proc *p = myproc();

  argaddr(0, &buf);
  argint(1, &len);
  argint(2, &mask);

  if (mask & ~(PTE_A | PTE_D))
    return -1;

  if (len < 0)
    return -1;

  if (buf + len < buf)
    return -1;

  if (buf + len > p->sz)
    return -1;

  uint64 st = PGROUNDDOWN(buf);
  uint64 end = PGROUNDDOWN(buf + len - 1);

  for (uint64 va = st; va <= end; va += PGSIZE)
  {
    pte_t *pte = walk(p->pagetable, va, 0);

    if (pte == 0 || (*pte & PTE_V) == 0)
      return -1;

    *pte &= ~mask;
  }

  return 0;
}

uint64
sys_checkflags(void)
{
  uint64 buf;
  int len, mask;
  struct proc *p = myproc();

  argaddr(0, &buf);
  argint(1, &len);
  argint(2, &mask);

  if (mask & ~(PTE_A | PTE_D))
    return -1;

  if (len < 0)
    return -1;

  if (buf + len < buf)
    return -1;

  if (buf + len > p->sz)
    return -1;

  uint64 st = PGROUNDDOWN(buf);
  uint64 end = PGROUNDDOWN(buf + len - 1);

  for (uint64 va = st; va <= end; va += PGSIZE)
  {
    pte_t *pte = walk(p->pagetable, va, 0);

    if (pte == 0 || (*pte & PTE_V) == 0)
      return -1;

    if (*pte & mask)
      return 1;
  }

  return 0;
}
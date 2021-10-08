#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "pstat.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
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

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
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

// set the number of lottery assignment ticket
// of the process.
uint64
sys_settickets(void)
{
  int n;

  if(argint(0, &n) < 0)
    return -1;
  if(n < 1)
    return -1;
  // There is no need to protect this access with a lock,
  // because this is the only way of modifying this value.
  myproc()->tickets = n;
  return 0;
}

uint64
sys_getpinfo(void)
{
  extern struct proc proc[NPROC];
  struct pstat procstat;
  uint64 useraddr;
  int i;

  if (argaddr(0, &useraddr) < 0)
    return -1;
  if (!useraddr)
    return -1;

  for(i = 0; i < NPROC; i++) {
    procstat.inuse[i] = proc[i].state != UNUSED;
    procstat.tickets[i] = proc[i].tickets;
    procstat.pid[i] = proc[i].pid;
    procstat.ticks[i] = proc[i].ticks;
  }
  return copyout(myproc()->pagetable, useraddr,
                 (char*) &procstat, sizeof(procstat));
}

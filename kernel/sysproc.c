#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"

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


uint64
sys_trace(void)
{
  int mask;
  struct proc *p = myproc();

  if(argint(0, &mask) < 0)
    return -1;
  p -> trace_mask = mask;
  return 0;
} 

uint64
sys_info(void)
{
  uint64 user_sysinfo_addr;
  struct sysinfo res;
  struct proc* p = myproc();

  if(argaddr(0, &user_sysinfo_addr) < 0)
    return -1;

  if(user_sysinfo_addr > p->sz)
    return -1;
  
  // get free physical mem
  uint64 free_byte_num = kres();

  res.freemem = free_byte_num;

  // get in-mem process number
  uint64 used_proc_num = usedproc();

  res.nproc = used_proc_num;
  copyout(p->pagetable, user_sysinfo_addr, (char *)&res, sizeof(struct sysinfo));

  return 0;
}
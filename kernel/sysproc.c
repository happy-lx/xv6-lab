#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

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


#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  uint64 result_mask = 0;

  uint64 user_addr_start;
  uint64 user_buf_addr;
  int page_num;

  struct proc* process = myproc();

  if(argaddr(0, &user_addr_start) < 0)
    return -1;
  if(argint(1, &page_num) < 0)
    return -1;
  if(argaddr(2, &user_buf_addr) < 0)
    return -1; 
  // printf("user_addr_start :%p\n",user_addr_start);
  user_addr_start = PGROUNDDOWN(user_addr_start);
  // printf("user_addr_start after ground :%p\n",user_addr_start);

  for(int i=0;i<page_num;i++) {
    pte_t* pte = walk(process->pagetable, user_addr_start, 0);
    // printf("pte %d is : %p\n",i,*pte);
    if((*pte & PTE_V) && (*pte & PTE_A)) {
      result_mask = result_mask | (1 << i);
      *pte = *pte & ~(PTE_A);
    }
    user_addr_start += PGSIZE;
  }
  // vmprint(process->pagetable);
  copyout(process->pagetable, user_buf_addr, (char*)&result_mask, sizeof(uint64));
  return 0;
}
#endif

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

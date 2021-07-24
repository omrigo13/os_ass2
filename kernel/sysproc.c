#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
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

// task 2.2.1 update kill sys call
uint64
sys_kill(void)
{
  int pid, signum;

  if(argint(0, &pid) < 0)
    return -1;
  if(argint(1, &signum) < 0)
    return -1;

  return kill(pid, signum);
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

// task 2.1.3
uint64
sys_sigprocmask(void)
{
  int sigmask;

  if(argint(0, &sigmask) < 0)
    return -1;

  return sigprocmask(sigmask);
}

// task 2.1.4
uint64
sys_sigaction(void)
{
  int signum;
  struct sigaction *act, *oldact;

  if(argint(0, &signum) < 0)
    return -1;
  if(argaddr(1, (void*)&act) < 0)
    return -1;
  if(argaddr(2, (void*)&oldact) < 0)
    return -1;

  return sigaction(signum, act, oldact);
}

// task 2.1.5
uint64
sys_sigret(void)
{
  sigret();
  return 0;
}

// task 3.2
uint64
sys_kthread_create(void)
{
  uint64 start_func;
  uint64 stack;
  
  if(argaddr(0, &start_func) < 0)
    return -1;
  if(argaddr(1, &stack) < 0)
    return -1;

  return kthread_create((void *) start_func, (void *) stack);
}

// task 3.2
uint64
sys_kthread_id(void)
{
  return kthread_id();
}

// task 3.2
uint64
sys_kthread_exit(void)
{
  int status;

  if(argint(0, &status) < 0)
    return -1;

  kthread_exit(status);
  return 0;
}
// task 3.2
uint64
sys_kthread_join(void)
{
  int thread_id;
  int *status;

  if(argint(0, &thread_id) < 0)
    return -1;
  if(argaddr(1, (uint64 *)&status) < 0)
    return -1;

  return kthread_join(thread_id, status);
}

// task 4.1
uint64
sys_bsem_alloc(void)
{
  return bsem_alloc();
}

// task 4.1
uint64
sys_bsem_free(void)
{
  int descriptor;

  if (argint(0, &descriptor) < 0)
    return -1;

  bsem_free(descriptor);
  return 0;
}

// task 4.1
uint64
sys_bsem_down(void)
{
  int descriptor;

  if (argint(0, &descriptor) < 0)
    return -1;

  bsem_down(descriptor);
  return 0;
}

// task 4.1
uint64
sys_bsem_up(void)
{
  int descriptor;

  if (argint(0, &descriptor) < 0)
    return -1;
    
  bsem_up(descriptor);
  return 0;
}
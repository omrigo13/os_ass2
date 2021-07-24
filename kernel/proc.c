#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

struct binary_semaphore Bsemaphores[MAX_BSEM]; // task 4 binary semphores

int nextpid = 1;
struct spinlock pid_lock;

// task 3
int nexttid = 1;
struct spinlock tid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// task 2.4 handling signals
extern void* signal_return_start(void);
extern void* signal_return_end(void);

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void
proc_mapstacks(pagetable_t kpgtbl) {
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    char *pa = kalloc();
    if(pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int) (p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

// task 4.1 init the binary semaphores
void 
init_binary_semaphores(){
  for(struct binary_semaphore *bsem = Bsemaphores; bsem < &Bsemaphores[MAX_BSEM]; bsem++)
    initlock(&bsem->lock, "binary_semaphore");
}

// initialize the proc table at boot time.
void
procinit(void)
{
  struct proc *p;
  struct thread *t;
  
  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  initlock(&tid_lock, "nexttid");
  init_binary_semaphores();
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");
      for(t = p->process_threads_table; t < &p->process_threads_table[NTHREAD]; t++) {
        initlock(&t->lock, "thread");
      }
      // p->kstack = KSTACK((int) (p - proc));
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu*
mycpu(void) {
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc*
myproc(void) {
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

// Return the current struct thread *, or zero if none.
// task 3.1, access to the current running thread
struct thread*
mythread(void) {
  push_off();
  struct cpu *c = mycpu();
  struct thread *t = c->thread;
  pop_off();
  return t;
}

int
allocpid() {
  int pid;
  
  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// task 3 allocate new thread id
int
alloctid() {
  int tid;
  
  acquire(&tid_lock);
  tid = nexttid;
  nexttid = nexttid + 1;
  release(&tid_lock);

  return tid;
}

// task 3 free a thread and all its content
static void
freethread(struct thread* t)
{
  if(t->user_trap_frame_backup)
    kfree((void*)t->user_trap_frame_backup);
  t->user_trap_frame_backup = 0;
  if(t->kstack)
    kfree((void*)t->kstack);
  t->kstack = 0;
  t->trapframe = 0;
  t->chan = 0;
  t->state = UNUSED_THREAD;
  t->xstate = 0;
  t->tid = 0;
  t->killed = 0;
  t->parent = 0;
  t->trap_frame_index = 0;
}

// task 3 allocate new thread
static struct thread*
allocthread(struct proc* p) 
{
  struct thread *t;
  int i = 0;
  for(t = p->process_threads_table; t < &p->process_threads_table[NTHREAD]; t++, i++) {
    if (t != mythread()) {
      acquire(&t->lock);
      if(t->state == UNUSED_THREAD) {
        goto found;
      } else {
        release(&t->lock);
      }
    }
  }
  return 0;

  found:
  t->tid = alloctid();
  t->state = USED_THREAD;
  t->trap_frame_index = i;
  t->trapframe = &p->thread_trapframes[i];
  t->parent = p;
  t->killed = 0;

  // Allocate a trapframe backup page.
  if((t->user_trap_frame_backup = (struct trapframe *)kalloc()) == 0){
    freethread(t);  
    release(&t->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&t->context, 0, sizeof(t->context));
  t->context.ra = (uint64)forkret;

  // Allocate kernel stack.
  if((t->kstack = (uint64)kalloc()) == 0) {
      freethread(t);
      release(&t->lock);
      return 0;
  }
  t->context.sp = t->kstack + PGSIZE;

  return t;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;

  // Allocate a trapframe page.
  if((p->thread_trapframes = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // // Allocate a trapframe backup page.
  // if((p->user_trap_frame_backup = (struct trapframe *)kalloc()) == 0){
  //   freeproc(p);
  //   release(&p->lock);
  //   return 0;
  // }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  // memset(&p->context, 0, sizeof(p->context));
  // p->context.ra = (uint64)forkret;
  // p->context.sp = p->kstack + PGSIZE;

  // task 2.1.2
  for (int i = 0; i < 32; i++)
    p->signal_handlers[i] = (void*) SIG_DFL; // init signal handlers for a process
  p->pending_signals = 0; // init pending signals for a process

   struct thread *t;
  // Allocate thread.
  if((t = allocthread(p)) == 0){
    release(&t->lock);
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  release(&t->lock);

  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  if(p->thread_trapframes)
    kfree((void*)p->thread_trapframes);
  p->thread_trapframes = 0;
  // if(p->user_trap_frame_backup)
  //   kfree((void*)p->user_trap_frame_backup);
  // p->user_trap_frame_backup = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  //p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
  p->pending_signals = 0;
  p->signal_mask = 0;

  for(int i = 0; i < 32; i++) {
    p->signal_handlers[i] = (void*) SIG_DFL;
    p->signal_handlers_masks[i] = 0;
  }

  p->signal_handler_flag = 0;
  p->signal_mask_backup = 0;

  struct thread *t;
  for(t = p->process_threads_table; t < &p->process_threads_table[NTHREAD]; t++) {
    freethread(t);
  }
}

// Create a user page table for a given process,
// with no user memory, but with trampoline pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe just below TRAMPOLINE, for trampoline.S.
  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->thread_trapframes), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// od -t xC initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// Set up first user process.
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;
  
  // allocate one user page and copy init's instructions
  // and data into it.
  uvminit(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->thread_trapframes->epc = 0;      // user program counter
  p->thread_trapframes->sp = PGSIZE;  // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // p->state = RUNNABLE;
  p->process_threads_table[0].state = RUNNABLE_THREAD;

  // release(&(p->process_threads_table[0].lock));
  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *p = myproc();
  acquire(&p->lock);
  sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc(p->pagetable, sz, sz + n)) == 0) {
      release(&p->lock);
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  release(&p->lock);
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();
  struct thread *nt;
  struct thread *t = mythread();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  nt = &np->process_threads_table[0];

  // copy saved user registers.
  // *(np->thread_trapframes) = *(p->thread_trapframes);

  // copy saved user registers.
  *(nt->trapframe) = *(t->trapframe);

  // Cause fork to return 0 in the child.
  nt->trapframe->a0 = 0;

  // task 2.1.2 copy signal mask and signal handlers from parent to child, pending signals doesn't inherited
  np->signal_mask = p->signal_mask;
  np->pending_signals = 0;

  for(i = 0; i < 32; i++) {
    np->signal_handlers[i] = p->signal_handlers[i];
    np->signal_handlers_masks[i] = p->signal_handlers_masks[i];
  }

  // Cause fork to return 0 in the child.
  // np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);
  // release(&nt->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  // acquire(&np->lock);
  // np->state = RUNNABLE;
  // release(&np->lock);
  acquire(&nt->lock);
  nt->state = RUNNABLE_THREAD;
  release(&nt->lock);

  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status)
{
  struct proc *p = myproc();
  struct thread *cur_t = mythread();

  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);
  
  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;

  for(struct thread *t = p->process_threads_table; t < &p->process_threads_table[NTHREAD]; t++) {
      if(t->tid != cur_t->tid && t->state != UNUSED_THREAD) {
          acquire(&t->lock);
          t->killed = 1;
          if (t->state == SLEEPING_THREAD)
              t->state = RUNNABLE_THREAD;
          release(&t->lock);
          kthread_join(t->tid, 0);
      }
  }

  release(&p->lock);

  acquire(&cur_t->lock);
  cur_t->state = ZOMBIE_THREAD;

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(uint64 addr)
{
  struct proc *np;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(np = proc; np < &proc[NPROC]; np++){
      if(np->parent == p){
        // make sure the child isn't still in exit() or swtch().
        acquire(&np->lock);

        havekids = 1;
        if(np->state == ZOMBIE){
          // Found one.
          pid = np->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&np->xstate,
                                  sizeof(np->xstate)) < 0) {
            release(&np->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(np);
          release(&np->lock);
          release(&wait_lock);
          return pid;
        }
        release(&np->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || p->killed){
      release(&wait_lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &wait_lock);  //DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct thread *t;
  struct cpu *c = mycpu();
  
  c->proc = 0;
  c->thread = 0;
  for(;;){
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();

    for(p = proc; p < &proc[NPROC]; p++) {
      // acquire(&p->lock);
      if(p->state == USED) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        for(t = p->process_threads_table; t < &p->process_threads_table[NTHREAD]; t++) {
          acquire(&t->lock);
          if(t->state == RUNNABLE_THREAD) {
            t->state = RUNNING_THREAD;
            c->thread = t;
            // p->state = RUNNING;
            c->proc = p;
            swtch(&c->context, &t->context);
            // Process is done running for now.
            // It should have changed its p->state before coming back.
            c->proc = 0;
            c->thread = 0;
          }
          release(&t->lock);
        }
      }
    }
  }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  // struct proc *p = myproc();
  struct thread *t = mythread();

  if(!holding(&t->lock))
    panic("sched t->lock");
  // if(!holding(&p->lock))
  //   panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(t->state == RUNNING_THREAD)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&t->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct thread *t = mythread();
  acquire(&t->lock);
  t->state = RUNNABLE_THREAD;
  sched();
  release(&t->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
  release(&mythread()->lock);

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    first = 0;
    fsinit(ROOTDEV);
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct thread *t = mythread();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&t->lock);  //DOC: sleeplock1
  release(lk);

  // Go to sleep.
  t->chan = chan;
  t->state = SLEEPING_THREAD;

  sched();

  // Tidy up.
  t->chan = 0;

  // Reacquire original lock.
  release(&t->lock);
  acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
  struct proc *p;
  struct thread *t;

  for(p = proc; p < &proc[NPROC]; p++) {
    for(t = p->process_threads_table; t < &p->process_threads_table[NTHREAD]; t++) {
      if(t != mythread()){
        acquire(&t->lock);
        if(t->state == SLEEPING_THREAD && t->chan == chan) {
          t->state = RUNNABLE_THREAD;
        }
        release(&t->lock);
      }
    }
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kill(int pid, int signum)
{
  struct proc *p;

  if(signum < 0 || signum > 31)
    return -1;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      if(p->state == ZOMBIE || p->state == UNUSED) {
        release(&p->lock);
        return -1;
      }

      // task 2.2.1
      p->pending_signals = p->pending_signals | (1 << signum);

      // if(p->state == SLEEPING && signum == SIGKILL){
      //   // Wake process from sleep().
      //   p->state = RUNNABLE;
      // }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  // [SLEEPING]  "sleep ", // task 3.1, no sleeping state for process, it works with threads instead
  // [RUNNABLE]  "runble", // task 3.1, no sleeping state for process, it works with threads instead
  // [RUNNING]   "run   ", // task 3.1, no sleeping state for process, it works with threads instead
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}

// task 2.1.3
uint
sigprocmask(uint sigmask)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  uint old_mask = p->signal_mask;
  p->signal_mask = sigmask;
  release(&p->lock);
  return old_mask;
}

// task 2.1.4
int
sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
  if(signum < 0 || signum > 31)
    return -1;
  if(signum == SIGKILL || signum == SIGSTOP)
    return -1;

  struct proc *p = myproc();
  acquire(&p->lock);
  int successful_mask;
  if(oldact != 0) {
    copyout(p->pagetable, (uint64)&oldact->sa_handler, (char*)&p->signal_handlers[signum], sizeof(p->signal_handlers[signum]));
    copyout(p->pagetable, (uint64)&oldact->sigmask, (char*)&p->signal_handlers_masks[signum], sizeof(p->signal_mask));
  }

  copyin(p->pagetable, (char *)&successful_mask, (uint64)&act->sigmask, sizeof(act->sigmask));

  if(act != 0) {
    if(((1 << SIGKILL) & successful_mask) || ((1 << SIGSTOP) & successful_mask))
    {
        release(&p->lock);
        return -1;
    }
  
    copyin(p->pagetable, (char *)&p->signal_handlers[signum], (uint64)&act->sa_handler, sizeof(act->sa_handler));
    p->signal_handlers_masks[signum] = successful_mask;
  }

  release(&p->lock);
  return 0;
}

//task 2.1.5
void
sigret()
{
  struct proc *p = myproc();
  struct thread *t = mythread();
  
  acquire(&p->lock);
  acquire(&t->lock);
  memmove(t->trapframe, t->user_trap_frame_backup, sizeof(struct trapframe));
  p->signal_mask = p->signal_mask_backup;
  p->signal_handler_flag = 0;
  release(&t->lock);
  release(&p->lock);
}

// task 2.3
void
SIGKILL_handler()
{
  struct proc *p = myproc();
  struct thread *t;
  p->killed = 1;

  // if(p->state == SLEEPING)
  //   p->state = RUNNABLE;
  for(t = p->process_threads_table; t < &p->process_threads_table[NTHREAD]; t++) {
    acquire(&t->lock);
    if(t->state == SLEEPING_THREAD){
      // Wake process from sleep().
      t->state = RUNNABLE_THREAD;
    }
    release(&t->lock);
  }  
}

void
SIGSTOP_handler()
{
  struct proc *p = myproc();

  while(1){
    if (!holding(&p->lock))
      acquire(&p->lock);
    
    if(!(p->pending_signals & (1 << SIGCONT))) {
      release(&p->lock);
      yield();
    } 
    else {
      break;
    }
  }

  p->pending_signals ^= (1 << SIGSTOP); // turns off the SIGSTOP signal from the pending signals using XOR operation
  p->pending_signals ^= (1 << SIGCONT); // turns off the SIGCONT signal from the pending signals using XOR operation
}

void
SIGCONT_handler()
{
  struct proc *p = myproc();
  // acquire(&p->lock);
  // if SIGSTOP does not exists in pending signals, turn off SIGCONT signal
  if (((1 << SIGSTOP) & p->pending_signals) == 0)
    p->pending_signals ^= (1 << SIGCONT); // turns off the SIGCONT signal from the pending signals using XOR operation
  // release(&p->lock);
}

void
handle_signals()
{  
  struct proc *p = myproc();
  acquire(&p->lock);
  if (p->signal_handler_flag){
    release(&p->lock);
    return;
  }
  for(int i = 0; i < 32; i++) {
    uint cur_sig = 1 << i;
    if((cur_sig & p->pending_signals) && !(cur_sig & p->signal_mask)) {
      if (p->signal_handlers[i] == (void*)SIG_IGN) {
        p->pending_signals ^= cur_sig;        // turns off the cur_sig signal from the pending signals using XOR operation
        continue;
      }
      if(p->signal_handlers[i] == (void*)SIG_DFL) {
        switch(i) {
          case SIGCONT:
            SIGCONT_handler();
            break;
          case SIGSTOP:
            SIGSTOP_handler();
            break;
          case SIGKILL:
            SIGKILL_handler();
            p->pending_signals ^= cur_sig;   // turns off the cur_sig signal from the pending signals using XOR operation
            break;
          default:
            SIGKILL_handler();
            p->pending_signals ^= cur_sig;  // turns off the cur_sig signal from the pending signals using XOR operation
            break;
        }
      }
      else { 
        struct thread *t = mythread();
        uint signal_return_size = signal_return_end - signal_return_start;           // calculate sigret size (using assembly injection for start and end)
        p->signal_handler_flag = 1;                                                  // block handling other signals 
        p->signal_mask_backup = p->signal_mask;                                      // backup current process mask
        p->signal_mask = p->signal_handlers_masks[i];                                // switch process mask with signal handler mask
        memmove(t->user_trap_frame_backup, t->trapframe, sizeof(struct trapframe));  // backup user trapframe
        t->trapframe->sp -= signal_return_size;                                      // allocate space for sigret in trapframe stack
        copyout(p->pagetable, (uint64)t->trapframe->sp, (char*)&signal_return_start, signal_return_size);  // copy sigret assembly injection to trapframe stack
        t->trapframe->a0 = i;                               // move signum to a0
        t->trapframe->ra = t->trapframe->sp;                // move sigret to ra
        p->pending_signals ^= cur_sig;                      // turns off the cur_sig signal from the pending signals using XOR operation
        t->trapframe->epc = (uint64)p->signal_handlers[i];  // set program counter to the signal handler 
      }
    }
  }
  release(&p->lock);
}

int
kthread_create( void ( *start_func ) ( ) , void *stack) 
{
  int tid;
  struct proc *p = myproc();
  struct thread* cur_t = mythread();
  struct thread* t = allocthread(p);

  if(t == 0)
    return -1;

  tid = t->tid;
  t->state = RUNNABLE_THREAD; // updates thread state to runnable
  *t->trapframe = *cur_t->trapframe;
  t->trapframe->sp = (uint64)(stack + MAX_STACK_SIZE - 16); // allocates a user stack for the new thread 
  t->trapframe->epc = (uint64)start_func; // updates program counter of the thread to start func

  release(&t->lock);
  return tid;
}

int
kthread_id() 
{
  return mythread()->tid;
}

void
kthread_exit(int status)
{
  struct proc *p = myproc();
  struct thread *cur_t = mythread();

  acquire(&p->lock);
  int active_threads = 0;
  for(struct thread *t = p->process_threads_table; t < &p->process_threads_table[NTHREAD]; t++) {
    acquire(&t->lock);
    if (t->tid != cur_t->tid && t->state != UNUSED_THREAD && t->state != ZOMBIE_THREAD)
      active_threads++;
    release(&t->lock);
  }

  // if this is the last thread running we should release process and exit
  if(active_threads == 0) {
    release(&p->lock);
    exit(status);
  }

  acquire(&cur_t->lock);
  cur_t->xstate = status;
  cur_t->state = ZOMBIE_THREAD;

  release(&p->lock);
  
  wakeup(cur_t);  // wake up the threads that are waiting for current thread to exit
  
  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

int
kthread_join(int thread_id, int *status)
{
  struct thread *join_thread  = 0;
  struct proc *p = myproc();

  // search for the tread that should be join
  for(struct thread *t = p->process_threads_table; t < &p->process_threads_table[NTHREAD]; t++) {
    acquire(&t->lock);
    if(thread_id == t->tid) {
      join_thread = t;
      goto found;
    }
    release(&t->lock);
  }

  return -1;

  found:
    // calling thread waits on target thread to finish running
    while(join_thread->state != ZOMBIE_THREAD && join_thread->state != UNUSED_THREAD)
      sleep(join_thread, &join_thread->lock);

    // once the joined thread finished running, get its status
    if(join_thread->state == ZOMBIE_THREAD) {
      if (status != 0 && copyout(p->pagetable, (uint64)status, (char *)&join_thread->xstate, sizeof(join_thread->xstate)) < 0) {
        release(&join_thread->lock);
        return -1;
      }
      freethread(join_thread);
    } 

    release(&join_thread->lock);
    return 0;
}

int 
bsem_alloc() 
{
  int descriptor;
  int flag_space = 0; // indicator of space for a new binary semaphore allocation
  for(descriptor = 0; !flag_space && descriptor < MAX_BSEM; descriptor++) {
    acquire(&Bsemaphores[descriptor].lock);
    if(!Bsemaphores[descriptor].occupied){  // search for the first semaphore that is not occupied to allocate it
      flag_space = 1;
      break;
    }
    release(&Bsemaphores[descriptor].lock);
  }
  if(!flag_space)  // in a situation of no place for allocate new semaphore
    return -1;
  Bsemaphores[descriptor].occupied = 1;     // the allocated semaphore occupied for using through the descriptor
  Bsemaphores[descriptor].value = 1;        // the allocated semaphore in unlocked state
  release(&Bsemaphores[descriptor].lock);
  return descriptor;
}

void 
bsem_free(int descriptor) 
{
  acquire(&Bsemaphores[descriptor].lock);
  Bsemaphores[descriptor].occupied = 0;   // free the semaphore for new allocation
  release(&Bsemaphores[descriptor].lock);
}

void 
bsem_down(int descriptor) 
{
  acquire(&Bsemaphores[descriptor].lock);
  if(Bsemaphores[descriptor].occupied) {          // checks if the binary semaphore already acquired
    while(Bsemaphores[descriptor].value == 0)     // block the binary semaphore until it is unlocked
      sleep(&Bsemaphores[descriptor], &Bsemaphores[descriptor].lock);
    Bsemaphores[descriptor].value = 0;            
  }
  release(&Bsemaphores[descriptor].lock);
}

void
bsem_up(int descriptor) 
{
  acquire(&Bsemaphores[descriptor].lock);
  if(Bsemaphores[descriptor].occupied) {
    Bsemaphores[descriptor].value = 1;            // releases the binary semaphore
    wakeup(&Bsemaphores[descriptor]);             
  }
  release(&Bsemaphores[descriptor].lock);
}
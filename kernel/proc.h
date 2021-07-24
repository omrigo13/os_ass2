// Saved registers for kernel context switches.
struct context {
  uint64 ra;
  uint64 sp;

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

// Per-CPU state.
struct cpu {
  struct proc *proc;          // The process running on this cpu, or null.
  struct context context;     // swtch() here to enter scheduler().
  int noff;                   // Depth of push_off() nesting.
  int intena;                 // Were interrupts enabled before push_off()?
  struct thread *thread;      // task 3.1, every process have 1 initial thread running in it
};

extern struct cpu cpus[NCPU];

// per-process data for the trap handling code in trampoline.S.
// sits in a page by itself just under the trampoline page in the
// user page table. not specially mapped in the kernel page table.
// the sscratch register points here.
// uservec in trampoline.S saves user registers in the trapframe,
// then initializes registers from the trapframe's
// kernel_sp, kernel_hartid, kernel_satp, and jumps to kernel_trap.
// usertrapret() and userret in trampoline.S set up
// the trapframe's kernel_*, restore user registers from the
// trapframe, switch to the user page table, and enter user space.
// the trapframe includes callee-saved user registers like s0-s11 because the
// return-to-user path via usertrapret() doesn't return through
// the entire kernel call stack.
struct trapframe {
  /*   0 */ uint64 kernel_satp;   // kernel page table
  /*   8 */ uint64 kernel_sp;     // top of process's kernel stack
  /*  16 */ uint64 kernel_trap;   // usertrap()
  /*  24 */ uint64 epc;           // saved user program counter
  /*  32 */ uint64 kernel_hartid; // saved kernel tp
  /*  40 */ uint64 ra;
  /*  48 */ uint64 sp;
  /*  56 */ uint64 gp;
  /*  64 */ uint64 tp;
  /*  72 */ uint64 t0;
  /*  80 */ uint64 t1;
  /*  88 */ uint64 t2;
  /*  96 */ uint64 s0;
  /* 104 */ uint64 s1;
  /* 112 */ uint64 a0;
  /* 120 */ uint64 a1;
  /* 128 */ uint64 a2;
  /* 136 */ uint64 a3;
  /* 144 */ uint64 a4;
  /* 152 */ uint64 a5;
  /* 160 */ uint64 a6;
  /* 168 */ uint64 a7;
  /* 176 */ uint64 s2;
  /* 184 */ uint64 s3;
  /* 192 */ uint64 s4;
  /* 200 */ uint64 s5;
  /* 208 */ uint64 s6;
  /* 216 */ uint64 s7;
  /* 224 */ uint64 s8;
  /* 232 */ uint64 s9;
  /* 240 */ uint64 s10;
  /* 248 */ uint64 s11;
  /* 256 */ uint64 t3;
  /* 264 */ uint64 t4;
  /* 272 */ uint64 t5;
  /* 280 */ uint64 t6;
};

enum procstate { UNUSED, USED, ZOMBIE }; //  no need for process states of SLEEPING, RUNNABLE, RUNNING (now it works with threads)

// task 3.1, thread states
enum threadstate { UNUSED_THREAD, USED_THREAD, SLEEPING_THREAD, RUNNABLE_THREAD, RUNNING_THREAD, ZOMBIE_THREAD };

// task 3, implementation of thread struct
struct thread {
  struct spinlock lock;

  // t->lock must be held when using these:
  enum threadstate state;        // Thread state
  void *chan;                    // If non-zero, sleeping on chan
  int killed;                    // If non-zero, have been killed
  int xstate;                    // Exit status to be returned to parent's wait
  int tid;                       // Thread ID

  // proc_tree_lock must be held when using this:
  struct proc *parent;           // Parent process of thread

  // these are private to the process, so p->lock need not be held.
  uint64 kstack;                              // Virtual address of kernel stack
  struct trapframe *trapframe;                // data page for trampoline.S
  struct context context;                     // swtch() here to run process

  int trap_frame_index;                       // Thread trapframe index, task 3.1
  struct trapframe *user_trap_frame_backup;   // user trapframe backup, task 2.4
};

// Per-process state
struct proc {
  struct spinlock lock;

  // p->lock must be held when using these:
  enum procstate state;        // Process state
  // void *chan;               // If non-zero, sleeping on chan, task 3.1 no need it anymore, transfered to thread struct
  int killed;                  // If non-zero, have been killed
  int xstate;                  // Exit status to be returned to parent's wait
  int pid;                     // Process ID

  // proc_tree_lock must be held when using this:
  struct proc *parent;         // Parent process

  // these are private to the process, so p->lock need not be held.
  // uint64 kstack;            // Virtual address of kernel stack, task 3.1 no need it anymore, transfered to thread struct
  uint64 sz;                   // Size of process memory (bytes)
  pagetable_t pagetable;       // User page table
  struct trapframe *thread_trapframes; // data page for trampoline.S, task 3.1
  // struct context context;   // swtch() here to run process, task 3.1 no need it anymore, transfered to thread struct
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)

  int signal_handler_flag;                  // flag to indicate whether the process is handling a signal and needs to block handling other signals, task 2.4
  uint signal_mask_backup;                  // saves previous ginal mask while running signal handler, tasks 2.1.5 , 2.4 

  uint signal_handlers_masks[32];           // signal handlers masks (mask per signal handler), task 2

  // task 2.1.1
  uint pending_signals;                           // pending signals
  uint signal_mask;                               // signal mask
  void *signal_handlers[32];                      // signal handlers
  struct thread process_threads_table[NTHREAD];   // threads running in this process
};

// task 2.1.4
struct sigaction {
  void (*sa_handler) (int); // signal handler
  uint sigmask;             // mask of signals to block during handling signal
};

// task 4.1
struct binary_semaphore {
    int occupied;           // binary semaphore belongs to a descriptor
    int value;              // value should be 0 or 1, 1 = wait operation works, 0 = signal operation succeeds
    struct spinlock lock;   // semaphore lock for operations on binary semaphore
};
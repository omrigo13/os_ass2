struct stat;
struct rtcdate;
struct sigaction;

// system calls
int fork(void);
int exit(int) __attribute__((noreturn));
int wait(int*);
int pipe(int*);
int write(int, const void*, int);
int read(int, void*, int);
int close(int);
int kill(int, int); // task 2.2.1 kill sys call will send the signal to the process pid
int exec(char*, char**);
int open(const char*, int);
int mknod(const char*, short, short);
int unlink(const char*);
int fstat(int fd, struct stat*);
int link(const char*, const char*);
int mkdir(const char*);
int chdir(const char*);
int dup(int);
int getpid(void);
char* sbrk(int);
int sleep(int);
int uptime(void);
uint sigprocmask(uint); // task 2.1.3 update process signal mask
int sigaction(int, const struct sigaction*, struct sigaction*); // task 2.1.4 register a new handler for a specific signal
void sigret(void); // task 2.1.5 restore the process to its original workflow
int kthread_create(void (*)(), void *); // task 3.2, creates a new thread within the context of the calling process
int kthread_id(void); // task 3.2, returns the caller thread's id
void kthread_exit(int); // task 3.2, terminates the execution of the calling thread
int kthread_join(int, int*); // task 3.2, suspends the execution of the calling thread until the target thread indicated by the argument thread id, terminates
int bsem_alloc(void); // task 4.1, allocates a new binary semaphore and returns its descriptor
void bsem_free(int); // task 4.1, frees the binary semaphore with the given descriptor
void bsem_down(int); // task 4.1, attempt to acquire the semaphore
void bsem_up(int); // task 4.1, releases the semaphore

// ulib.c
int stat(const char*, struct stat*);
char* strcpy(char*, const char*);
void *memmove(void*, const void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
void fprintf(int, const char*, ...);
void printf(const char*, ...);
char* gets(char*, int max);
uint strlen(const char*);
void* memset(void*, int, uint);
void* malloc(uint);
void free(void*);
int atoi(const char*);
int memcmp(const void *, const void *, uint);
void *memcpy(void *, const void *, uint);

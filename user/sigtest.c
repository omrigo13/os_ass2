#include "../kernel/param.h"
#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "user.h"
#include "../kernel/fs.h"
#include "../kernel/fcntl.h"
#include "../kernel/syscall.h"
#include "../kernel/memlayout.h"
#include "../kernel/riscv.h"

struct sigaction
{
    void (*sa_handler)(int);
    uint sigmask;
};

void f(void)
{
    return;
}

void sig_handler(int signum)
{
    char st[26] = "handling the situation!!!\n";
    write(1, st, 26);
}

void test_sigkill()
{
    int pid = fork();
    if (pid == 0)
    {
        sleep(5);
        for (int i = 0; i < 30; i++)
            printf("about to get killed\n");
    }
    else
    {
        printf("parent send signal to to kill child\n");
        printf("kill ret= %d\n", kill(pid, SIGKILL));
        printf("parent wait for child\n");
        wait(0);
        printf("parent: child is dead\n");
        sleep(10);
        // exit(0);
    }
}

void test_stop_cont()
{
    int pid = fork();
    int i;
    if (pid == 0)
    {
        for (i = 0; i < 10; i++)
            printf("%d\n ", i);
        exit(0);
    }
    else
    {
        sleep(1);
        // printf("son pid=%d, dad pid=%d\n",pid, getpid());
        // printf("parent send signal to to stop child\n");
        kill(pid, SIGSTOP);
        // printf("parent: go to sleep \n");
        // sleep(5);
        // for(int i=0;i<10;i++)
        //     printf("parent..");

        kill(pid, SIGCONT);
        printf("***\n");
        wait(0);
        for (int i = 0; i < 100; i++)
            printf("parent..");
    }
}
void test_usersig()
{
    int pid = fork();
    if (pid == 0)
    {
        struct sigaction act;
        act.sa_handler = &sig_handler;
        act.sigmask = 0;
        struct sigaction oldact;
        sigaction(3, &act, &oldact);
        sleep(10);
        exit(0);
    }
    else
    {
        sleep(1);
        kill(pid, 3);
        wait(0);
    }
}

void test_block()
{

    int pid = fork();
    if (pid == 0)
    {
        sigprocmask(1 << 30 | 1 << 31);
        sleep(0);
        for (int i = 0; i < 10; i++)
        {
            printf("child blocking signal :-)\n");
        }
        exit(0);
    }
    else
    {
        sleep(1);
        kill(pid, 30);
        kill(pid, 31);
        wait(0);
    }
}

void test_ignore()
{
    int pid = fork();
    int signum = 22;
    if (pid == 0)
    {
        struct sigaction newAct;
        struct sigaction *oldAct = (struct sigaction *)malloc(sizeof(struct sigaction));
        newAct.sigmask = 0;
        newAct.sa_handler = (void *)SIG_IGN;
        int ans = sigaction(signum, &newAct, oldAct);
        printf("ans from sigaction %d, old act returned is: mask= %d address= %d \n", ans, oldAct->sigmask, (uint64)oldAct->sa_handler);

        sleep(6);
        for (int i = 0; i < 10; i++)
        {
            printf("child ignoring signal :-)\n");
        }
        exit(0);
    }
    else
    {
        sleep(10);
        kill(pid, signum);
        wait(0);
    }
}

int main()
{
    printf("-----------------------------test_sigkill-----------------------------\n");
    test_sigkill();

    printf("-----------------------------test_stop_cont_sig-----------------------------\n");
    test_stop_cont();

    printf("0x%x\n", &f);

    printf("-----------------------------test_usersig-----------------------------\n");
    test_usersig();

    printf("-----------------------------test_block-----------------------------\n");
    test_block();
    printf("-----------------------------test_ignore-----------------------------\n");
    test_ignore();
    exit(0);
    return 0;
}
#include "kernel/param.h"
#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/riscv.h"


#include "kernel/spinlock.h"  // NEW INCLUDE FOR ASS2
#include "kernel/proc.h"         // NEW INCLUDE FOR ASS 2, has all the signal definitions and sigaction definition.  Alternatively, copy the relevant things into user.h and include only it, and then no need to include spinlock.h .


void sig_handler_start(){
    return;
}

void handler1() {}

void handler2() {}

void child_handler() {}

// check that we can update the sigaction with new act
int
sigaction_update_mask() {
    int test_status = 1;
    struct sigaction* sigaction_1 = malloc(sizeof(struct sigaction*));
    struct sigaction* sigaction_2 = malloc(sizeof(struct sigaction*));
    sigaction_1->sa_handler = &handler1;
    sigaction_1->sigmask = 1;

    if(sigaction(2, sigaction_1, 0) == -1)
        test_status = 0;

    sigaction(2, 0, sigaction_2);

    if(sigaction_2->sigmask != 1)
        test_status = 0;

    sigaction_1->sa_handler = (void*)SIG_DFL;
    sigaction_1->sigmask = 0;
    for(int i = 0; i < 32; i++)
        if(sigaction(i, sigaction_1, 0) < 0)
            test_status = 0;

   return test_status;
}

// check that the update does not happens when the sigmask is SIGSTOP or SIGKILL or with illigal signum
int
sigaction_update_mask_bad() {
    int test_status = 1;
    struct sigaction* sigaction_1 = malloc(sizeof(struct sigaction*));

    sigaction_1->sigmask = (1 << SIGSTOP);
    if(sigaction(2, sigaction_1, 0) < 0)
        test_status = 0;

    sigaction_1->sigmask = (1 << SIGKILL);
    if(sigaction(2, sigaction_1, 0) < 0)
        test_status = 0;
    
    sigaction_1->sigmask = 0;
    if(sigaction(-8, sigaction_1, 0) < 0)
        test_status = 0;

    if(sigaction(SIGSTOP, sigaction_1, 0) < 0)
        test_status = 0;
    
    if(sigaction(SIGKILL, sigaction_1, 0) < 0)
        test_status = 0;    

    return test_status;
}

// checks that the mask returned is the old mask and the new mask updated
int 
sigprocmask_update() {
    
    int test_status = 1;

    uint mask1 = sigprocmask((1 << 7) | (1 << 6));
    if(mask1 != 0)
        test_status = 0;

    uint mask2 = sigprocmask((1 << 2) | (1 << 3) | (1 << 5));
    if(mask2 != ((1 << 4) | (1 << 3)))
        test_status = 0;
    
    int pid = fork();
    if(pid == 0){
        uint mask1_child = sigprocmask((1 << 7) | (1 << 6));
        if(mask1_child != ((1 << 2) | (1 << 3) | (1 << 5)))
            test_status = 0;
        else {
            char* argv[] = {"task_2e_tests", "sigprocmask", 0};
            if (exec(argv[0], argv) < 0)
                test_status = 0;
        }
    }

    int status;
    wait(&status);
    sigprocmask(0);
    return (test_status & status);
}

// checks update sigaction using handlers
int 
sigaction_handler_update() {
    int test_status = 1;
    struct sigaction* sigaction_1 = malloc(sizeof(struct sigaction*));
    struct sigaction* sigaction_2 = malloc(sizeof(struct sigaction*));
    struct sigaction* sigaction_3 = malloc(sizeof(struct sigaction*));
    sigaction_1->sa_handler = &handler1;
    sigaction_1->sigmask = 1;
    
    if(sigaction(2, sigaction_1, sigaction_2) < 0)
        test_status = 0;

    if(sigaction_2->sa_handler != (void*)SIG_DFL)
        test_status = 0;

    if(sigaction(2, 0, sigaction_2) < 0)
        test_status = 0;

    if(sigaction_2->sa_handler != &handler1)
        test_status = 0;

    sigaction_1->sa_handler = &handler2;
    sigaction(3, sigaction_1, 0);
    sigaction_1->sa_handler = (void*)SIG_IGN;
    sigaction(4, sigaction_1, 0);

    int pid = fork();
    if (pid == 0) {
        sigaction(2, 0, sigaction_3);
        if (sigaction_3->sa_handler != &handler1)
            test_status = 0;

        sigaction(3, 0, sigaction_3);
        if (sigaction_3->sa_handler != &handler2)
            test_status = 1;

        sigaction(4, 0, sigaction_3);
        if (sigaction_3->sa_handler != (void*)SIG_IGN)
            test_status = 0;

        char* argv[] = {"task_2e_tests", "sigaction_and_handler", 0};
        if(exec(argv[0],argv) < 0)
            test_status = 0;
    }

    int status;
    wait(&status);
    sigaction_1->sa_handler = (void*)SIG_DFL;
    sigaction_1->sigmask = 0;

    return test_status & status;
}

// checks sending signals working with SIGKILL signal
int 
sending_signals() {
    int test_status = 0;
    int status;
    int pid = fork();
    if (pid == 0)
        while(1)
            sleep(5);
    else {
        kill(pid, SIGKILL);
        wait(&status);
    }
    return test_status & status;
}

// checks stop and cont signals working
int 
sending_signals_2() {
    int test_status = 0;
    struct sigaction* sigaction_1 = malloc(sizeof(struct sigaction*));
    sigaction_1->sa_handler = &child_handler;
    sigaction_1->sigmask = 0;
    if(sigaction(4, sigaction_1, 0) < 0)
        test_status = 0;
    
    int pid = fork();

    if(pid == 0){
        while(1){}
    }

    sleep(10);
    kill(pid, SIGSTOP);
    sleep(5);
    kill(pid, 4);
    sleep(5);
    kill(pid, SIGCONT);
    sleep(10);
    kill(pid, SIGKILL);
    int status;
    wait(&status);

    return test_status & status;
}

// checks multiple processes using handlers and update sigaction
int
multiple_processes() { 
    int test_status = 0;
    struct sigaction* sigaction_1 = malloc(sizeof(struct sigaction*));
    struct sigaction* sigaction_2 = malloc(sizeof(struct sigaction*));
    sigaction_1->sa_handler = &handler1;
    sigaction_1->sigmask = 0;
    sigaction_2->sa_handler = &handler2;
    sigaction_2->sigmask = 0;

    if(sigaction(4, sigaction_1, 0) < 0)
        test_status = 0;
    if(sigaction(3, sigaction_2, 0) < 0)
        test_status = 0;

    for(int i = 0; i < 16; i++) {
        int pid = fork();
        if(pid == 0){
            if(i % 2 == 0)
                kill(getpid(), 4);
            else if(i % 2 == 1)
                kill(getpid(), 3);
            exit(0);
        } 
        else {
            wait(0);
        }
    }
    return test_status;
}

// checks blocking signals using handlers and update sigaction
int 
blocking_signals(){
    int test_status = 0;
    struct sigaction* sigaction_1 = malloc(sizeof(struct sigaction*));
    sigaction_1->sa_handler = &handler2;
    sigaction_1->sigmask = 0;

    if(sigaction(2, sigaction_1, 0) == -1)
        printf("sigaction failed\n");

    int pid = fork();
    if(pid == 0){
        sigprocmask(4);
        while(1){}
    } else {
        sleep(5);
        kill(pid, 2);
        sleep(10);
        kill(pid, SIGKILL);
    }

    int status;
    wait(&status);
    return test_status;
}

int
main(int argc, char *argv[]){

    struct test {
    int (*f)();
    char *s;
    } tests[] = {
        {sigaction_update_mask, "sigaction_update_mask"},
        {sigaction_update_mask_bad, "sigaction_update_mask_bad"},
        {sigprocmask_update, "sigprocmask_update"},
        {sigaction_handler_update, "sigaction_handler_update"},
        {sending_signals, "sending_signals"},
        {sending_signals_2, "sending_signals_2"},
        {multiple_processes, "multiple_processes"},
        {blocking_signals, "blocking_signals"},
        {0,0}
    };

    struct sigaction* sigaction_1 = malloc(sizeof(struct sigaction*));

    if(argc > 1) {
        if(strcmp(argv[1], "sigprocmask") == 0) {
            uint exec_mask = sigprocmask(0);
            if(exec_mask != ((1 << 7) | (1 << 6))) {
                exit(1);
            }
            exit(0);
        }
        if(strcmp(argv[1],"sigaction_and_handler") == 0){
            sigaction(2, 0, sigaction_1);
            if(sigaction_1->sa_handler != (void*)SIG_DFL)
                exit(1);
    
            sigaction(3, 0, sigaction_1);
            if(sigaction_1->sa_handler != (void*)SIG_DFL)
                exit(1);

            sigaction(4, 0, sigaction_1);
            if(sigaction_1->sa_handler != (void*)SIG_IGN)
                exit(1);
            exit(0);
        }
    }
    for(struct test *t = tests; t->s != 0; t++) {
        printf("%s: ", t->s);
        int test_status = (t->f)();
        if(!test_status)
            printf("OK\n");
        else {
            printf("FAIL\n");
            exit(-1);
        }
    }
    printf("ALL TESTS PASSED\n");
    exit(0);  
}
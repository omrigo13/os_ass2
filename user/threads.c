#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// #define print(s) printf("%s\n", s);
// #define STACK_SIZE 4000
// #define NTHREADS 2
// int shared = 0;
// void func()
// {
//     sleep(1);
//     shared++;
//     kthread_exit(7);
// }

// int main(int argc, char *argv[])
// {
//     int tids[NTHREADS];
//     void *stacks[NTHREADS];
//     for (int i = 0; i < NTHREADS; i++)
//     {
//         void *stack = malloc(STACK_SIZE);
//         tids[i] = kthread_create(func, stack);
//         stacks[i] = stack;
//     }

//     for (int i = 0; i < NTHREADS; i++)
//     {
//         int status;
//         kthread_join(tids[i], &status);
//         free(stacks[i]);
//         printf("the status is: %d\n", status);
//     }
//     printf("%d\n", shared);
//     exit(0);
// }

void func1(int s1, int s2){
    printf("S1\n");
    bsem_up(s2);
    printf("S5\n");
    bsem_up(s2);
    printf("S8\n");
    printf("S9\n");
    bsem_down(s1);
    printf("S7\n");
    bsem_up(s2);
}   

void func2(int s1, int s2){
    bsem_down(s2);
    printf("S2\n");
    printf("S3\n");
    bsem_down(s2);
    printf("S6\n");
    bsem_up(s1);
    bsem_down(s2);
    printf("S4\n");
    bsem_up(s2);
}

int main(){
    int s1 = bsem_alloc();
    int s2 = bsem_alloc();
    bsem_down(s1);
    bsem_down(s2);
    // printf("S1: %d S2: %d\n", s1, s2);

    if (s1 < 0 || s2 < 0){
        printf("bsem_alloc failed\n");
    }

    int pid = fork();

    if(pid == 0){
        func1(s1, s2);
    }
    else{
        func2(s1, s2);
    }
    printf("need to print: 1 5 8 9 2 3 6 7 4\n");
    exit(0);
}
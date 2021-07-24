#include "kernel/types.h"
#include "user/user.h"
#include "user/Csemaphore.h"

int 
csem_alloc(struct counting_semaphore *sem, int initial_value) {
    
    sem->s1 = bsem_alloc();
    sem->s2 = bsem_alloc();

    if(sem->s1 < 0 || sem->s2 < 0)          // no binary semaphores to make counting semaphore
        return -1;

    if(initial_value >= 0) {
        if(initial_value == 0)
            bsem_down(sem->s2);
        sem->value = initial_value;
    }
    return 0;
}

void 
csem_free(struct counting_semaphore *sem) {

    bsem_free(sem->s1);
    bsem_free(sem->s2);
    free(sem);
}

void 
csem_down(struct counting_semaphore *sem) {

    bsem_down(sem->s2);
    bsem_down(sem->s1);
    sem->value--;
    if(sem->value > 0)
        bsem_up(sem->s2);
    bsem_up(sem->s1);
}

void 
csem_up(struct counting_semaphore *sem) {

    bsem_down(sem->s1);
    sem->value++;
    if(sem->value == 1)
        bsem_up(sem->s2);
    bsem_up(sem->s1);
}

struct counting_semaphore {
    int s1;
    int s2;
    int value;
};

int csem_alloc(struct counting_semaphore *sem, int initial_value);
void csem_free(struct counting_semaphore *sem);
void csem_down(struct counting_semaphore *sem);
void csem_up(struct counting_semaphore *sem);
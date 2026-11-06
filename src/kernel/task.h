#ifndef TASK_H
#define TASK_H

#define TASK_MAX_COUNT 4U
#define TASK_STACK_SIZE 8192U

enum task_state {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_TERMINATED
};

typedef void (*task_entry_fn)(void* argument);

void task_scheduler_init(void);
int task_create(task_entry_fn entry, void* argument);
void task_scheduler_run_once(void);
void task_yield(void);
int task_demo_start(void);
void task_demo_get_counters(unsigned int* background_count,
                            unsigned int* status_tick);

int task_find_next_ready(const enum task_state* states,
                         unsigned int count,
                         int after_index);
unsigned int* task_build_initial_stack(void* stack, unsigned int size,
                                       void (*bootstrap)(void));

#endif

#include "task.h"
#include "heap.h"
#include "timer.h"
#include <stdint.h>

#define TASK_NONE (-1)
#define INITIAL_CONTEXT_WORDS 5U

struct task {
    enum task_state state;
    task_entry_fn entry;
    void* argument;
    void* stack_base;
    unsigned int stack_size;
    unsigned int* saved_stack;
};

static struct task tasks[TASK_MAX_COUNT];
static unsigned int* scheduler_stack;
static int current_task = TASK_NONE;
static int last_scheduled_task = TASK_NONE;
static volatile unsigned int background_count;
static volatile unsigned int status_tick;
static volatile unsigned int status_update_count;
static volatile unsigned int exit_probe_count;

extern void task_context_switch(unsigned int** old_stack,
                                unsigned int* new_stack);

static void task_bootstrap(void);
static void background_counter_task(void* argument);
static void status_updater_task(void* argument);
static void exit_probe_task(void* argument);

int task_find_next_ready(const enum task_state* states,
                         unsigned int count,
                         int after_index) {
    unsigned int index;
    unsigned int scanned;

    if (!states || !count || count > 0x7FFFFFFFU ||
        after_index < TASK_NONE ||
        (after_index >= 0 && (unsigned int)after_index >= count)) {
        return TASK_NONE;
    }

    index = after_index < 0 ? 0U : (unsigned int)after_index + 1U;
    if (index == count) index = 0;

    for (scanned = 0; scanned < count; scanned++) {
        if (states[index] == TASK_READY) return (int)index;
        index++;
        if (index == count) index = 0;
    }

    return TASK_NONE;
}

unsigned int* task_build_initial_stack(void* stack, unsigned int size,
                                       void (*bootstrap)(void)) {
    uintptr_t start = (uintptr_t)stack;
    uintptr_t top;
    unsigned int* context;
    unsigned int i;

    if (!stack || !bootstrap || start > UINTPTR_MAX - size ||
        size < INITIAL_CONTEXT_WORDS * sizeof(unsigned int) + 15U) {
        return 0;
    }

    top = (start + size) & ~((uintptr_t)15U);
    if (top < start ||
        top - start < INITIAL_CONTEXT_WORDS * sizeof(unsigned int) +
                      sizeof(unsigned int)) {
        return 0;
    }
    top -= sizeof(unsigned int);
    if (top - start < INITIAL_CONTEXT_WORDS * sizeof(unsigned int)) {
        return 0;
    }

    context = (unsigned int*)(top -
                              INITIAL_CONTEXT_WORDS * sizeof(unsigned int));
    for (i = 0; i < INITIAL_CONTEXT_WORDS - 1U; i++) {
        context[i] = 0;
    }
    context[INITIAL_CONTEXT_WORDS - 1U] =
        (unsigned int)(uintptr_t)bootstrap;
    return context;
}

static void release_task(unsigned int index) {
    struct task* task = &tasks[index];
    void* stack = task->stack_base;

    task->state = TASK_UNUSED;
    task->entry = 0;
    task->argument = 0;
    task->stack_base = 0;
    task->stack_size = 0;
    task->saved_stack = 0;
    kfree(stack);
}

void task_scheduler_init(void) {
    unsigned int i;

    for (i = 0; i < TASK_MAX_COUNT; i++) {
        tasks[i].state = TASK_UNUSED;
        tasks[i].entry = 0;
        tasks[i].argument = 0;
        tasks[i].stack_base = 0;
        tasks[i].stack_size = 0;
        tasks[i].saved_stack = 0;
    }
    scheduler_stack = 0;
    current_task = TASK_NONE;
    last_scheduled_task = TASK_NONE;
    background_count = 0;
    status_tick = 0;
    status_update_count = 0;
    exit_probe_count = 0;
}

int task_create(task_entry_fn entry, void* argument) {
    unsigned int index;
    void* stack;
    unsigned int* initial_stack;

    if (!entry || current_task != TASK_NONE) return TASK_NONE;

    for (index = 0; index < TASK_MAX_COUNT; index++) {
        if (tasks[index].state == TASK_UNUSED) break;
    }
    if (index == TASK_MAX_COUNT) return TASK_NONE;

    stack = kmalloc(TASK_STACK_SIZE);
    if (!stack) return TASK_NONE;

    initial_stack = task_build_initial_stack(stack, TASK_STACK_SIZE,
                                             task_bootstrap);
    if (!initial_stack) {
        kfree(stack);
        return TASK_NONE;
    }

    tasks[index].entry = entry;
    tasks[index].argument = argument;
    tasks[index].stack_base = stack;
    tasks[index].stack_size = TASK_STACK_SIZE;
    tasks[index].saved_stack = initial_stack;
    tasks[index].state = TASK_READY;
    return (int)index;
}

void task_scheduler_run_once(void) {
    enum task_state states[TASK_MAX_COUNT];
    unsigned int i;
    int next_task;

    if (current_task != TASK_NONE) return;

    for (i = 0; i < TASK_MAX_COUNT; i++) {
        if (tasks[i].state == TASK_TERMINATED) release_task(i);
        states[i] = tasks[i].state;
    }

    next_task = task_find_next_ready(states, TASK_MAX_COUNT,
                                     last_scheduled_task);
    if (next_task == TASK_NONE) return;

    current_task = next_task;
    last_scheduled_task = next_task;
    tasks[next_task].state = TASK_RUNNING;
    task_context_switch(&scheduler_stack, tasks[next_task].saved_stack);

    if (tasks[next_task].state == TASK_TERMINATED) {
        release_task((unsigned int)next_task);
    }
    current_task = TASK_NONE;
}

void task_yield(void) {
    int yielding_task = current_task;

    if (yielding_task == TASK_NONE ||
        tasks[yielding_task].state != TASK_RUNNING ||
        !scheduler_stack) {
        return;
    }

    tasks[yielding_task].state = TASK_READY;
    task_context_switch(&tasks[yielding_task].saved_stack, scheduler_stack);
}

static void task_bootstrap(void) {
    int running_task = current_task;

    if (running_task == TASK_NONE ||
        tasks[running_task].state != TASK_RUNNING) {
        for (;;) __asm__ volatile ("hlt");
    }

    tasks[running_task].entry(tasks[running_task].argument);
    tasks[running_task].state = TASK_TERMINATED;
    task_context_switch(&tasks[running_task].saved_stack, scheduler_stack);

    for (;;) __asm__ volatile ("hlt");
}

static void background_counter_task(void* argument) {
    (void)argument;
    for (;;) {
        background_count++;
        task_yield();
    }
}

static void status_updater_task(void* argument) {
    (void)argument;
    for (;;) {
        status_tick = timer_get_ticks();
        status_update_count++;
        task_yield();
    }
}

static void exit_probe_task(void* argument) {
    (void)argument;
    exit_probe_count++;
}

int task_demo_start(void) {
    int background_task;
    int status_task;
    int probe_task;
    unsigned int i;

    background_count = 0;
    status_tick = 0;
    status_update_count = 0;
    exit_probe_count = 0;
    background_task = task_create(background_counter_task, 0);
    if (background_task == TASK_NONE) return 0;

    status_task = task_create(status_updater_task, 0);
    if (status_task == TASK_NONE) {
        release_task((unsigned int)background_task);
        return 0;
    }
    probe_task = task_create(exit_probe_task, 0);
    if (probe_task == TASK_NONE) {
        release_task((unsigned int)background_task);
        release_task((unsigned int)status_task);
        return 0;
    }

    task_scheduler_run_once();
    task_scheduler_run_once();
    task_scheduler_run_once();
    task_scheduler_run_once();
    task_scheduler_run_once();
    if (background_count == 2U && status_update_count == 2U &&
        exit_probe_count == 1U &&
        tasks[probe_task].state == TASK_UNUSED) {
        return 1;
    }

    for (i = 0; i < TASK_MAX_COUNT; i++) {
        if (tasks[i].state != TASK_UNUSED) {
            release_task(i);
        }
    }
    return 0;
}

void task_demo_get_counters(unsigned int* counter,
                            unsigned int* tick) {
    if (counter) *counter = background_count;
    if (tick) *tick = status_tick;
}

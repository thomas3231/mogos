#include "timer.h"
#include "keyboard.h"   // reuses your existing outb/inb

extern void timer_handler_asm(void);

volatile unsigned int tick_count = 0;
volatile unsigned int uptime_seconds = 0;
volatile unsigned int idle_ticks = 0;
volatile unsigned int busy_ticks = 0;
volatile unsigned int cpu_usage_percent = 0;
volatile unsigned int idle_state = 0;
static unsigned int ticks_in_sample = 0;

void timer_handler_c(void) {
    tick_count++;
    ticks_in_sample++;

    if (idle_state) {
        idle_ticks++;
    } else {
        busy_ticks++;
    }

    if (ticks_in_sample == 100) {
        uptime_seconds++;
        if (idle_ticks + busy_ticks != 0) {
            cpu_usage_percent = (busy_ticks * 100) / (idle_ticks + busy_ticks);
        }
        idle_ticks = 0;
        busy_ticks = 0;
        ticks_in_sample = 0;
    }
}

void timer_install(void) {
    unsigned int divisor = 1193182 / 100;

    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);

    set_idt_gate(32, (unsigned int)timer_handler_asm);
}

unsigned int timer_get_ticks(void) {
    return tick_count;
}

unsigned int timer_get_uptime_seconds(void) {
    return uptime_seconds;
}

unsigned int timer_get_cpu_usage_percent(void) {
    return cpu_usage_percent;
}

void timer_set_idle_state(unsigned int is_idle) {
    idle_state = is_idle;
}

void timer_wait(unsigned int ticks) {
    unsigned int start = tick_count;
    while ((unsigned int)(tick_count - start) < ticks) {
        __asm__ volatile ("hlt");
    }
}
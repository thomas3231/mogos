#include "timer.h"
#include "keyboard.h"   // reuses your existing outb/inb

extern void timer_handler_asm(void);

volatile unsigned int tick_count = 0;

void timer_handler_c(void) {
    tick_count++;
}

void timer_install(void) {
    // PIT runs at 1193182 Hz by default; divide it down to 100 Hz (a tick every 10ms)
    unsigned int divisor = 1193182 / 100;

    outb(0x43, 0x36);                     // channel 0, mode 3, binary
    outb(0x40, divisor & 0xFF);           // low byte
    outb(0x40, (divisor >> 8) & 0xFF);    // high byte

    set_idt_gate(32, (unsigned int)timer_handler_asm);   // IRQ0 = interrupt 0x20 = 32
}

unsigned int timer_get_ticks(void) {
    return tick_count;
}

void timer_wait(unsigned int ticks) {
    unsigned int target = tick_count + ticks;
    while (tick_count < target) {
        __asm__ volatile ("hlt");
    }
}
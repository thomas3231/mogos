#include "graphics.h"
#include "keyboard.h"
#include "timer.h"
#include "heap.h"
#include "palette.h"
#include "exceptions.h"

extern void keyboard_handler_asm(void);

void kernel_main(void) {
    screen_init();
    heap_init();

    idt_install();
    exceptions_install();
    pic_remap();
    set_idt_gate(33, (unsigned int)keyboard_handler_asm);
    timer_install();

    fill_screen(0x10, 0x10, 0x10);
    render_status_frame();

    __asm__ volatile ("sti");

    for (;;) {
        timer_set_idle_state(1);
        __asm__ volatile ("hlt");
        timer_set_idle_state(0);
        render_status_frame();
    }
}
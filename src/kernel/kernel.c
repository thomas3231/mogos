#include "graphics.h"
#include "keyboard.h"
#include "timer.h"
#include "heap.h"
#include "palette.h"
#include "exceptions.h"
#include "rtc.h"
#include "speaker.h"
#include "ata.h"
#include "desktop.h"
#include "task.h"

extern void keyboard_handler_asm(void);


#include "splash.h"

void kernel_main(void) {
    screen_init();
    heap_init();
    backbuffer_init();
    task_scheduler_init();

    idt_install();
    exceptions_install();
    pic_remap();
    set_idt_gate(33, (unsigned int)keyboard_handler_asm);
    timer_install();

    if (!task_demo_start()) {
        fill_screen(0x80, 0x00, 0x00);
        static const char message[] = "TASK INIT FAILED";
        unsigned int i;
        for (i = 0; message[i]; i++) {
            draw_char(20 + (int)i * 18, 20, message[i],
                      0xFF, 0xFF, 0xFF, 2);
        }
        screen_swap();
        __asm__ volatile ("cli");
        for (;;) __asm__ volatile ("hlt");
    }

    __asm__ volatile ("sti");

    splash_run();
    in_splash = 0;

    desktop_init();
    desktop_draw();

    for (;;) {
        timer_set_idle_state(1);
        __asm__ volatile ("hlt");
        timer_set_idle_state(0);
        desktop_update();
        task_scheduler_run_once();
    }
}
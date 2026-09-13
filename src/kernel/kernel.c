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

extern void keyboard_handler_asm(void);


#include "splash.h"

void kernel_main(void) {
    screen_init();
    heap_init();
    backbuffer_init();

    idt_install();
    exceptions_install();
    pic_remap();
    set_idt_gate(33, (unsigned int)keyboard_handler_asm);
    timer_install();

    __asm__ volatile ("sti");

    splash_run();
    in_splash = 0;

    desktop_init();
    desktop_draw();

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
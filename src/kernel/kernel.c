#include "graphics.h"
#include "keyboard.h"

// Tell the compiler this assembly function exists elsewhere
extern void keyboard_handler_asm(void);

void kernel_main(void) {
    // 1. Initialize Screen
    screen_init();
    fill_screen(0x10, 0x10, 0x10);
    
    // Draw borders
    draw_rect(0, 0, screen.width, 4, 0x7C, 0xFC, 0x00);
    draw_rect(0, screen.height - 4, screen.width, 4, 0x7C, 0xFC, 0x00);
    draw_rect(0, 0, 4, screen.height, 0x7C, 0xFC, 0x00);
    draw_rect(screen.width - 4, 0, 4, screen.height, 0x7C, 0xFC, 0x00);

    // 2. Initialize Interrupts & Keyboard
    idt_install();
    pic_remap();
    set_idt_gate(33, (unsigned int)keyboard_handler_asm);
    __asm__ volatile ("sti");

    // 3. Idle Loop
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
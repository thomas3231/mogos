#include "graphics.h"
#include "keyboard.h"
#include "timer.h"
#include "heap.h"
#include "window.h"
#include "palette.h"
#include "mouse.h"

extern void keyboard_handler_asm(void);
extern void timer_handler_asm(void);

void kernel_main(void) {
    screen_init();
    heap_init();

    fill_screen(0x10, 0x10, 0x10);

    draw_rect(0, 0, screen.width, 4, palette[COLOR_GRASSGREEN][0], palette[COLOR_GRASSGREEN][1], palette[COLOR_GRASSGREEN][2]);
    draw_rect(0, screen.height - 4, screen.width, 4, palette[COLOR_GRASSGREEN][0], palette[COLOR_GRASSGREEN][1], palette[COLOR_GRASSGREEN][2]);
    draw_rect(0, 0, 4, screen.height, palette[COLOR_GRASSGREEN][0], palette[COLOR_GRASSGREEN][1], palette[COLOR_GRASSGREEN][2]);
    draw_rect(screen.width - 4, 0, 4, screen.height, palette[COLOR_GRASSGREEN][0], palette[COLOR_GRASSGREEN][1], palette[COLOR_GRASSGREEN][2]);

    idt_install();
    pic_remap();
    set_idt_gate(33, (unsigned int)keyboard_handler_asm);
    timer_install();
    mouse_install();
    __asm__ volatile ("sti");

    window_init(100, 80, 400, 300, "Terminal");
    window_draw();

    int last_x = mouse.x, last_y = mouse.y;

    for (;;) {
        __asm__ volatile ("hlt");
        if (mouse.x != last_x || mouse.y != last_y) {
            window_draw();       // redraw window (crude, but erases old cursor position)
            draw_cursor();
            last_x = mouse.x;
            last_y = mouse.y;
        }
    }
}
#include "mouse.h"
#include "keyboard.h"   // reuses outb/inb, set_idt_gate
#include "graphics.h"
#include "palette.h"

struct mouse_state mouse;

unsigned char mouse_cycle = 0;
signed char mouse_packet[3];

extern void mouse_handler_asm(void);

void mouse_wait(unsigned char type) {
    unsigned int timeout = 100000;
    if (type == 0) {
        while (timeout--) {
            if ((inb(0x64) & 1) == 1) return;
        }
    } else {
        while (timeout--) {
            if ((inb(0x64) & 2) == 0) return;
        }
    }
}

void mouse_write(unsigned char data) {
    mouse_wait(1);
    outb(0x64, 0xD4);
    mouse_wait(1);
    outb(0x60, data);
}

unsigned char mouse_read(void) {
    mouse_wait(0);
    return inb(0x60);
}

void mouse_install(void) {
    mouse.x = 320;
    mouse.y = 240;
    mouse.left_button = 0;
    mouse.right_button = 0;

    mouse_wait(1);
    outb(0x64, 0xA8);           // enable the auxiliary (mouse) device

    mouse_wait(1);
    outb(0x64, 0x20);           // request current controller config byte
    mouse_wait(0);
    unsigned char status = inb(0x60);
    status |= 2;                // set bit 1: enable IRQ12
    status &= ~0x20;            // clear bit 5: disable mouse clock (must be off to enable)

    mouse_wait(1);
    outb(0x64, 0x60);           // write new config byte
    mouse_wait(1);
    outb(0x60, status);

    mouse_write(0xF6);          // set defaults
    mouse_read();                // ack

    mouse_write(0xF4);          // enable data reporting
    mouse_read();                // ack

    set_idt_gate(44, (unsigned int)mouse_handler_asm);   // IRQ12 = interrupt 0x2C = 44
}

void mouse_handler_c(void) {
    unsigned char data = inb(0x60);

    switch (mouse_cycle) {
        case 0:
            mouse_packet[0] = data;
            mouse_cycle++;
            break;
        case 1:
            mouse_packet[1] = data;
            mouse_cycle++;
            break;
        case 2:
            mouse_packet[2] = data;
            mouse_cycle = 0;

            mouse.left_button = mouse_packet[0] & 0x01;
            mouse.right_button = mouse_packet[0] & 0x02;

            mouse.x += mouse_packet[1];
            mouse.y -= mouse_packet[2];   // Y is inverted

            if (mouse.x < 0) mouse.x = 0;
            if (mouse.y < 0) mouse.y = 0;
            if (mouse.x >= screen.width) mouse.x = screen.width - 1;
            if (mouse.y >= screen.height) mouse.y = screen.height - 1;

            break;
    }
}

void draw_cursor(void) {
    // simple filled triangle-ish arrow, ~8x8
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col <= row; col++) {
            put_pixel(mouse.x + col, mouse.y + row, palette[COLOR_WHITE][0], palette[COLOR_WHITE][1], palette[COLOR_WHITE][2]);
        }
    }
}
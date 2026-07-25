#include "keyboard.h"
#include "graphics.h"

struct idt_entry {
    unsigned short base_low;
    unsigned short selector;
    unsigned char always0;
    unsigned char flags;
    unsigned short base_high;
} __attribute__((packed));

struct idt_ptr {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

struct idt_entry idt[256];
struct idt_ptr idtp;

extern void idt_load(unsigned int);
extern void keyboard_handler_asm(void);

void set_idt_gate(int n, unsigned int handler) {
    idt[n].base_low = handler & 0xFFFF;
    idt[n].base_high = (handler >> 16) & 0xFFFF;
    idt[n].selector = 0x08;
    idt[n].always0 = 0;
    idt[n].flags = 0x8E;
}

void idt_install(void) {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (unsigned int)&idt + 0x20000;
    idt_load((unsigned int)&idtp);
}

void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void pic_remap(void) {
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0xFC);   // unmask both IRQ0 (timer) and IRQ1 (keyboard) 
    outb(0xA1, 0xEF);   // unmask IRQ12 (bit 4) on the slave PIC
}

char scancode_to_ascii[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0,' '
};



int ctrl_pressed = 0;
int alt_pressed = 0;
int shift_pressed = 0;

// scancodes: left ctrl=0x1D, left alt=0x38, left shift=0x2A, right shift=0x36
void keyboard_handler_c(void) {
    unsigned char scancode = inb(0x60);
    int is_release = scancode & 0x80;
    unsigned char code = scancode & 0x7F;

    if (code == 0x1D) { ctrl_pressed = !is_release; return; }
    if (code == 0x38) { alt_pressed = !is_release; return; }
    if (code == 0x2A || code == 0x36) { shift_pressed = !is_release; return; }

    if (is_release) return;

    // --- global shortcut interception happens BEFORE normal typing ---
    if (ctrl_pressed && alt_pressed) {
        if (code == 0x14) { /* 'T' */ spawn_terminal(); return; }
        if (code == 0x2E) { /* 'C' */ close_active_window(); return; }
        if (code == 0x32) { /* 'M' */ toggle_maximize(); return; }
        if (code == 0x2D) { /* 'X' */ kill_active_task(); return; }
    }

    // normal typing, unchanged
    char c = scancode_to_ascii[scancode];
    if (c) {
        print_char(c);
    }
}

void spawn_terminal(void) {
    // TODO: implement once window system exists
}

void close_active_window(void) {
    // TODO: implement once window system exists
}

void toggle_maximize(void) {
    // TODO: implement once window system exists
}

void kill_active_task(void) {
    // TODO: implement once window system exists
}





#include "keyboard.h"

#include "desktop.h"

#include "splash.h"

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
    outb(0xA1, 0xFF);
}

char scancode_to_ascii[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0,' '
};

static const char shifted_scancode_to_ascii[128] = {
    [2] = '!', [3] = '@', [4] = '#', [5] = '$', [6] = '%',
    [7] = '^', [8] = '&', [9] = '*', [10] = '(', [11] = ')',
    [12] = '_', [13] = '+', [26] = '{', [27] = '}',
    [39] = ':', [40] = '"', [41] = '~', [43] = '|',
    [51] = '<', [52] = '>', [53] = '?'
};

static int ctrl_pressed = 0;
static int alt_pressed = 0;
static int left_shift_pressed = 0;
static int right_shift_pressed = 0;
static int caps_lock_enabled = 0;
static int caps_lock_pressed = 0;



// keyboard.c
void outw(unsigned short port, unsigned short val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

unsigned short inw(unsigned short port) {
    unsigned short ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}




int extended_scancode = 0;

static char translate_scancode(unsigned char code) {
    char c = scancode_to_ascii[code];
    int shift_pressed = left_shift_pressed || right_shift_pressed;

    if (c >= 'a' && c <= 'z') {
        if (shift_pressed != caps_lock_enabled) c -= 'a' - 'A';
    } else if (shift_pressed && shifted_scancode_to_ascii[code]) {
        c = shifted_scancode_to_ascii[code];
    }

    return c;
}


int in_splash = 1;   // starts true, set false once splash_run() returns

void keyboard_handler_c(void) {
    unsigned char scancode = inb(0x60);

    if (scancode == 0xE0) {
        extended_scancode = 1;
        return;
    }

    int is_release = scancode & 0x80;
    unsigned char code = scancode & 0x7F;

    if (code == 0x1D) { ctrl_pressed = !is_release; extended_scancode = 0; return; }
    if (code == 0x38) { alt_pressed = !is_release; extended_scancode = 0; return; }
    if (code == 0x2A) { left_shift_pressed = !is_release; extended_scancode = 0; return; }
    if (code == 0x36) { right_shift_pressed = !is_release; extended_scancode = 0; return; }
    if (code == 0x3A) {
        if (is_release) {
            caps_lock_pressed = 0;
        } else if (!caps_lock_pressed) {
            caps_lock_enabled = !caps_lock_enabled;
            caps_lock_pressed = 1;
        }
        extended_scancode = 0;
        return;
    }

    if (is_release) { extended_scancode = 0; return; }

    if (in_splash) {
        if (code == 0x1C) {   // Enter
            splash_enter_pressed = 1;
        }
        extended_scancode = 0;
        return;
    }

    if (extended_scancode) {
        extended_scancode = 0;
        if (code == 0x48) { desktop_handle_key(1); return; }
        if (code == 0x50) { desktop_handle_key(2); return; }
        if (code == 0x4B) { desktop_handle_key(3); return; }
        if (code == 0x4D) { desktop_handle_key(4); return; }
        return;
    }

    if (ctrl_pressed && code == 0x23) {
        go_home();
        return;
    }
    if (ctrl_pressed && code == 0x1F) {
        desktop_handle_key(19);
        return;
    }
    if (ctrl_pressed && code == 0x18) {
        desktop_handle_key(15);
        return;
    }

    char c = translate_scancode(code);
    if (c) {
        desktop_handle_key(c);
    }
}

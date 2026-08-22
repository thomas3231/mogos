#include "exceptions.h"
#include "keyboard.h"   // set_idt_gate
#include "graphics.h"
#include "palette.h"

extern void exception_stub_0(void);
extern void exception_stub_1(void);
extern void exception_stub_2(void);
extern void exception_stub_3(void);
extern void exception_stub_4(void);
extern void exception_stub_5(void);
extern void exception_stub_6(void);
extern void exception_stub_7(void);
extern void exception_stub_8(void);
extern void exception_stub_13(void);
extern void exception_stub_14(void);

const char* exception_names[] = {
    "Divide by Zero", "Debug", "Non-Maskable Interrupt", "Breakpoint",
    "Overflow", "Bound Range Exceeded", "Invalid Opcode", "Device Not Available",
    "Double Fault", "", "", "", "", "General Protection Fault", "Page Fault"
};

void exception_handler_c(int vector) {
    fill_screen(0x00, 0x00, 0x80);

    const char* name = (vector < 15) ? exception_names[vector] : "Unknown Exception";

    int x = 20, y = 20;
    for (int i = 0; name[i]; i++) {
        draw_char(x, y, name[i], 0xFF, 0xFF, 0xFF, 2);
        x += 18;
    }

    __asm__ volatile ("cli");
    for (;;) { __asm__ volatile ("hlt"); }
}

void exceptions_install(void) {
    set_idt_gate(0, (unsigned int)exception_stub_0);
    set_idt_gate(1, (unsigned int)exception_stub_1);
    set_idt_gate(2, (unsigned int)exception_stub_2);
    set_idt_gate(3, (unsigned int)exception_stub_3);
    set_idt_gate(4, (unsigned int)exception_stub_4);
    set_idt_gate(5, (unsigned int)exception_stub_5);
    set_idt_gate(6, (unsigned int)exception_stub_6);
    set_idt_gate(7, (unsigned int)exception_stub_7);
    set_idt_gate(8, (unsigned int)exception_stub_8);
    set_idt_gate(13, (unsigned int)exception_stub_13);
    set_idt_gate(14, (unsigned int)exception_stub_14);
}
#ifndef KEYBOARD_H
#define KEYBOARD_H

void set_idt_gate(int n, unsigned int handler);
void idt_install(void);
void pic_remap(void);
void keyboard_handler_c(void);

#endif
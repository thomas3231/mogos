#ifndef KEYBOARD_H
#define KEYBOARD_H

void set_idt_gate(int n, unsigned int handler);
void idt_install(void);
void pic_remap(void);
void keyboard_handler_c(void);

void outb(unsigned short port, unsigned char val);
unsigned char inb(unsigned short port);

void spawn_terminal(void);
void close_active_window(void);
void toggle_maximize(void);
void kill_active_task(void);

// keyboard.h
void outw(unsigned short port, unsigned short val);
unsigned short inw(unsigned short port);

#endif
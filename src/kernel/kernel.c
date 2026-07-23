int cursor_row = 0;
int cursor_col = 0;

void scroll_screen(void) {
    char* video = (char*) 0x98000;

    // move every row up by one (row 1 -> row 0, row 2 -> row 1, etc.)
    for (int row = 1; row < 25; row++) {
        for (int col = 0; col < 80; col++) {
            int dest = ((row - 1) * 80 + col) * 2;
            int src = (row * 80 + col) * 2;
            video[dest] = video[src];
            video[dest + 1] = video[src + 1];
        }
    }

    // clear the now-empty last row
    for (int col = 0; col < 80; col++) {
        int offset = (24 * 80 + col) * 2;
        video[offset] = ' ';
        video[offset + 1] = 0x0F;
    }

    cursor_row = 24;
}

void print(char* msg) {
    char* video = (char*) 0x98000;

    for (int i = 0; msg[i] != '\0'; i++) {
        if (msg[i] == '\n') {
            cursor_row++;
            cursor_col = 0;
        } else {
            int offset = (cursor_row * 80 + cursor_col) * 2;
            video[offset] = msg[i];
            video[offset + 1] = 0x0F;
            cursor_col++;

            if (cursor_col >= 80) {
                cursor_col = 0;
                cursor_row++;
            }
        }

        if (cursor_row >= 25) {
            scroll_screen();
        }
    }
}

//tipkovnicna
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

extern void idt_load(unsigned int);   // defined in assembly, loads the IDT

void set_idt_gate(int n, unsigned int handler) {
    idt[n].base_low = handler & 0xFFFF;
    idt[n].base_high = (handler >> 16) & 0xFFFF;
    idt[n].selector = 0x08;       // your CODE_SEG selector
    idt[n].always0 = 0;
    idt[n].flags = 0x8E;          // present, ring 0, 32-bit interrupt gate
}

void idt_install(void) {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (unsigned int)&idt + 0x20000;   // compensate for kernel's physical load address
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
    outb(0x21, 0xFD);   // mask everything except IRQ1 (keyboard)
    outb(0xA1, 0xFF);
}

char scancode_to_ascii[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0,' '
};

extern void keyboard_handler_asm(void);

void keyboard_handler_c(void) {
    unsigned char scancode = inb(0x60);

    if (scancode & 0x80) return;   // key release, ignore

    char c = scancode_to_ascii[scancode];
    if (c) {
        char str[2] = { c, '\0' };
        print(str);
    }
}

//kraj tipkovnice

void kernel_main(void) {
    char* video = (char*) 0x98000;

    for (int i = 0; i < 80 * 25; i++) {
        video[i * 2] = ' ';
        video[i * 2 + 1] = 0x0F;
    }

    print("Kernel loaded. Try typing!\n");

    idt_install();
    set_idt_gate(0x21, (unsigned int)keyboard_handler_asm);
    pic_remap();
    __asm__ volatile ("sti");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
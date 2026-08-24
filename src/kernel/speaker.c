#include "speaker.h"
#include "keyboard.h"

void beep(unsigned int frequency) {
    if (frequency == 0) {
        nosound();
        return;
    }

    unsigned int divisor = 1193182 / frequency;

    outb(0x43, 0xB6);
    outb(0x42, divisor & 0xFF);
    outb(0x42, (divisor >> 8) & 0xFF);

    unsigned char tmp = inb(0x61);
    if (tmp != (tmp | 3)) {
        outb(0x61, tmp | 3);
    }
}

void nosound(void) {
    unsigned char tmp = inb(0x61) & 0xFC;
    outb(0x61, tmp);
}
#include "speaker.h"
#include "keyboard.h"
#include "timer.h"

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

void play_note(unsigned int freq, unsigned int duration_ticks, unsigned int gap_ticks) {
    beep(freq);
    timer_wait(duration_ticks);
    nosound();
    timer_wait(gap_ticks);
}

void play_boot_jingle(void) {
    play_note(262, 10, 2);
    play_note(330, 10, 2);
    play_note(392, 10, 2);
    play_note(523, 14, 4);

    play_note(262, 10, 2);
    play_note(330, 10, 2);
    play_note(440, 10, 2);
    play_note(392, 14, 4);

    play_note(659, 10, 2);
    play_note(587, 10, 2);
    play_note(523, 16, 0);
}
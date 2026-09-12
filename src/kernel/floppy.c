#include "floppy.h"
#include "keyboard.h"   // outb/inb

#define FDC_DOR   0x3F2
#define FDC_MSR   0x3F4
#define FDC_DATA  0x3F5

void floppy_wait_ready(void) {
    unsigned int timeout = 1000000;
    while (timeout--) {
        if (inb(FDC_MSR) & 0x80) return;
    }
}

void floppy_motor_on(void) {
    outb(FDC_DOR, 0x1C);   // drive 0 select, motor on, DMA/reset bits set
    for (volatile int i = 0; i < 500000; i++) { }   // brief spin-up delay
}

void floppy_motor_off(void) {
    outb(FDC_DOR, 0x0C);
}

// Converts an LBA to CHS, matching the same math your bootloader's
// lba_to_chs routine already performs (18 sectors/track, 2 heads).
void lba_to_chs_kernel(unsigned int lba, unsigned int* cylinder, unsigned int* head, unsigned int* sector) {
    *sector = (lba % 18) + 1;
    *head = (lba / 18) % 2;
    *cylinder = (lba / 18) / 2;
}

int floppy_read_sector(unsigned int lba, unsigned char* buffer) {
    unsigned int cylinder, head, sector;
    lba_to_chs_kernel(lba, &cylinder, &head, &sector);

    floppy_motor_on();

    outb(FDC_DATA, 0xE6);           // READ command, MFM + skip deleted-data bit
    floppy_wait_ready();
    outb(FDC_DATA, (head << 2));    // head select + drive 0
    floppy_wait_ready();
    outb(FDC_DATA, cylinder);
    floppy_wait_ready();
    outb(FDC_DATA, head);
    floppy_wait_ready();
    outb(FDC_DATA, sector);
    floppy_wait_ready();
    outb(FDC_DATA, 2);              // 512 bytes/sector
    floppy_wait_ready();
    outb(FDC_DATA, 18);             // last sector number on this track
    floppy_wait_ready();
    outb(FDC_DATA, 0x1B);           // gap length
    floppy_wait_ready();
    outb(FDC_DATA, 0xFF);           // data length (unused when sector size != 0)

    // NOTE: a real driver waits for an IRQ6 interrupt here to know the
    // transfer finished. We haven't wired that IRQ yet -- see below.
    for (volatile int i = 0; i < 2000000; i++) { }

    for (int i = 0; i < 512; i++) {
        buffer[i] = inb(FDC_DATA);
    }

    floppy_motor_off();
    return 1;
}
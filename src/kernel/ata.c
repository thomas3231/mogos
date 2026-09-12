#include "ata.h"
#include "keyboard.h"   // outb/inb/outw/inw

#define ATA_DATA        0x1F0
#define ATA_ERROR       0x1F1
#define ATA_SECCOUNT    0x1F2
#define ATA_LBA_LOW     0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HIGH    0x1F5
#define ATA_DRIVE_HEAD  0x1F6
#define ATA_STATUS      0x1F7
#define ATA_COMMAND     0x1F7

void ata_wait_ready(void) {
    while (inb(ATA_STATUS) & 0x80) { }   // wait while BSY (busy) bit is set
}

void ata_wait_drq(void) {
    while (!(inb(ATA_STATUS) & 0x08)) { }   // wait for DRQ (data request) bit
}

int ata_read_sector(unsigned int lba, unsigned char* buffer) {
    ata_wait_ready();

    outb(ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));   // master drive, LBA mode, top 4 bits of LBA
    outb(ATA_SECCOUNT, 1);
    outb(ATA_LBA_LOW, lba & 0xFF);
    outb(ATA_LBA_MID, (lba >> 8) & 0xFF);
    outb(ATA_LBA_HIGH, (lba >> 16) & 0xFF);
    outb(ATA_COMMAND, 0x20);   // READ SECTORS command

    ata_wait_ready();
    ata_wait_drq();

    unsigned short* buf16 = (unsigned short*)buffer;
    for (int i = 0; i < 256; i++) {
        buf16[i] = inw(ATA_DATA);
    }

    return 1;
}

int ata_write_sector(unsigned int lba, unsigned char* buffer) {
    ata_wait_ready();

    outb(ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SECCOUNT, 1);
    outb(ATA_LBA_LOW, lba & 0xFF);
    outb(ATA_LBA_MID, (lba >> 8) & 0xFF);
    outb(ATA_LBA_HIGH, (lba >> 16) & 0xFF);
    outb(ATA_COMMAND, 0x30);   // WRITE SECTORS command

    ata_wait_ready();
    ata_wait_drq();

    unsigned short* buf16 = (unsigned short*)buffer;
    for (int i = 0; i < 256; i++) {
        outw(ATA_DATA, buf16[i]);
    }

    return 1;
}
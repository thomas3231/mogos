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
#define ATA_ALT_STATUS  0x3F6

#define ATA_STATUS_BSY  0x80
#define ATA_STATUS_DRQ  0x08
#define ATA_STATUS_ERR  0x01
#define ATA_STATUS_DF   0x20
#define ATA_STATUS_ERROR_MASK (ATA_STATUS_ERR | ATA_STATUS_DF)
#define ATA_LBA_LIMIT   0x10000000U
#define ATA_POLL_LIMIT  1000000U

static int ata_wait_ready(void) {
    for (unsigned int timeout = 0; timeout < ATA_POLL_LIMIT; timeout++) {
        unsigned char status = inb(ATA_STATUS);
        if (status == 0 || status == 0xFF) return 0;
        if (!(status & ATA_STATUS_BSY)) {
            return !(status & ATA_STATUS_ERROR_MASK);
        }
    }
    return 0;
}

static int ata_wait_drq(void) {
    for (unsigned int timeout = 0; timeout < ATA_POLL_LIMIT; timeout++) {
        unsigned char status = inb(ATA_STATUS);
        if (status == 0 || status == 0xFF) return 0;
        if (status & ATA_STATUS_ERROR_MASK) return 0;
        if (!(status & ATA_STATUS_BSY) && (status & ATA_STATUS_DRQ)) return 1;
    }
    return 0;
}

static int ata_wait_complete(void) {
    for (unsigned int timeout = 0; timeout < ATA_POLL_LIMIT; timeout++) {
        unsigned char status = inb(ATA_STATUS);
        if (status == 0 || status == 0xFF) return 0;
        if (status & ATA_STATUS_ERROR_MASK) return 0;
        if (!(status & (ATA_STATUS_BSY | ATA_STATUS_DRQ))) return 1;
    }
    return 0;
}

static int ata_select_lba(unsigned int lba) {
    if (lba >= ATA_LBA_LIMIT) return 0;

    outb(ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    for (int i = 0; i < 4; i++) inb(ATA_ALT_STATUS);
    if (!ata_wait_ready()) return 0;

    outb(ATA_SECCOUNT, 1);
    outb(ATA_LBA_LOW, lba & 0xFF);
    outb(ATA_LBA_MID, (lba >> 8) & 0xFF);
    outb(ATA_LBA_HIGH, (lba >> 16) & 0xFF);
    return 1;
}

int ata_read_sector(unsigned int lba, unsigned char* buffer) {
    if (!buffer || !ata_select_lba(lba)) return 0;
    outb(ATA_COMMAND, 0x20);
    if (!ata_wait_drq()) return 0;

    for (int i = 0; i < 256; i++) {
        unsigned short word = inw(ATA_DATA);
        buffer[i * 2] = word & 0xFF;
        buffer[i * 2 + 1] = word >> 8;
    }

    return ata_wait_complete();
}

int ata_write_sector(unsigned int lba, unsigned char* buffer) {
    if (!buffer || !ata_select_lba(lba)) return 0;
    outb(ATA_COMMAND, 0x30);
    if (!ata_wait_drq()) return 0;

    for (int i = 0; i < 256; i++) {
        unsigned short word = buffer[i * 2] | ((unsigned short)buffer[i * 2 + 1] << 8);
        outw(ATA_DATA, word);
    }

    if (!ata_wait_complete()) return 0;
    outb(ATA_COMMAND, 0xE7);
    return ata_wait_complete();
}

int ata_get_sector_count(unsigned int* sector_count) {
    if (!sector_count) return 0;

    outb(ATA_DRIVE_HEAD, 0xA0);
    for (int i = 0; i < 4; i++) inb(ATA_ALT_STATUS);
    if (!ata_wait_ready()) return 0;
    if (inb(ATA_LBA_MID) != 0 || inb(ATA_LBA_HIGH) != 0) return 0;

    outb(ATA_COMMAND, 0xEC);
    if (!ata_wait_drq()) return 0;

    unsigned int lba28_sector_count = 0;
    for (int i = 0; i < 256; i++) {
        unsigned short word = inw(ATA_DATA);
        if (i == 60) lba28_sector_count = word;
        if (i == 61) lba28_sector_count |= (unsigned int)word << 16;
    }
    if (!ata_wait_complete() ||
        lba28_sector_count == 0 ||
        lba28_sector_count > ATA_LBA_LIMIT) {
        return 0;
    }

    *sector_count = lba28_sector_count;
    return 1;
}
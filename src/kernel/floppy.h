#ifndef FLOPPY_H
#define FLOPPY_H

int floppy_read_sector(unsigned int lba, unsigned char* buffer);

#endif
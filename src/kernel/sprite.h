#ifndef SPRITE_H
#define SPRITE_H

#define MGS_MAGIC_0 'M'
#define MGS_MAGIC_1 'G'
#define MGS_MAGIC_2 'S'
#define MGS_MAGIC_3 '1'

struct mgs_header {
    unsigned char magic[4];
    unsigned short width;
    unsigned short height;
    unsigned short sprite_count;
    unsigned short reserved;
};

struct sprite {
    unsigned short width;
    unsigned short height;
    unsigned char* pixels;   // width * height bytes, palette indices
};

// Loads sprite_index from a raw in-memory buffer already containing an .mgs
// file's bytes (e.g. read from disk via ata_read_sector, or a compiled-in array).
// Returns 1 on success, 0 on a bad/mismatched header.
int mgs_load_from_buffer(unsigned char* buffer, unsigned int sprite_index, struct sprite* out);

// Draws a loaded sprite at (x, y) using the existing palette table.
void draw_sprite(int x, int y, struct sprite* spr);

#endif
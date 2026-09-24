#include "sprite.h"
#include "graphics.h"
#include "palette.h"

int mgs_load_from_buffer(unsigned char* buffer, unsigned int sprite_index, struct sprite* out) {
    struct mgs_header* header = (struct mgs_header*)buffer;

    if (header->magic[0] != MGS_MAGIC_0 || header->magic[1] != MGS_MAGIC_1 ||
        header->magic[2] != MGS_MAGIC_2 || header->magic[3] != MGS_MAGIC_3) {
        return 0;   // not a valid .mgs file
    }

    if (sprite_index >= header->sprite_count) {
        return 0;   // asked for a sprite that doesn't exist in this file
    }

    unsigned int frame_size = header->width * header->height;
    unsigned char* data_start = buffer + sizeof(struct mgs_header);

    out->width = header->width;
    out->height = header->height;
    out->pixels = data_start + (sprite_index * frame_size);

    return 1;
}

void draw_sprite(int x, int y, struct sprite* spr) {
    for (int row = 0; row < spr->height; row++) {
        for (int col = 0; col < spr->width; col++) {
            unsigned char index = spr->pixels[row * spr->width + col];
            // index 255 reserved as "transparent" -- skip drawing that pixel
            if (index == 255) continue;

            unsigned char r, g, b;
            palette_lookup(index, &r, &g, &b);   // see note below
            put_pixel(x + col, y + row, r, g, b);
        }
    }
}
#ifndef GRAPHICS_H
#define GRAPHICS_H

struct screen_info {
    unsigned char* framebuffer;
    unsigned short width;
    unsigned short height;
    unsigned short pitch;
    unsigned char bpp;
};

extern struct screen_info screen;
extern unsigned char* backbuffer;

void screen_init(void);
void put_pixel(int x, int y, unsigned char r, unsigned char g, unsigned char b);
void draw_rect(int x, int y, int width, int height, unsigned char r, unsigned char g, unsigned char b);
void fill_screen(unsigned char r, unsigned char g, unsigned char b);

// Add the scale parameter to draw_char
void draw_char(int x, int y, char c, unsigned char r, unsigned char g, unsigned char b, int scale);
void print_char(char c);

void draw_status_bar(void);
void draw_text_line(int x, int y, const char* text);
void clear_status_text_area(void);
void draw_status_text(void);
void render_status_frame(void);


void backbuffer_init(void);
void screen_swap(void);

extern void fast_copy(void* dest, void* src, unsigned int dword_count);

#endif
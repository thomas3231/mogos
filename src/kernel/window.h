#ifndef WINDOW_H
#define WINDOW_H

struct window {
    int x, y, width, height;
    char title[32];
    int focused;
};

extern struct window active_window;

void window_init(int x, int y, int w, int h, char* title);
void window_draw(void);

#endif
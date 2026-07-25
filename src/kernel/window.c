#include "window.h"
#include "graphics.h"
#include "palette.h"

struct window active_window;

void window_init(int x, int y, int w, int h, char* title) {
    active_window.x = x;
    active_window.y = y;
    active_window.width = w;
    active_window.height = h;
    active_window.focused = 1;

    int i = 0;
    while (title[i] && i < 31) {
        active_window.title[i] = title[i];
        i++;
    }
    active_window.title[i] = '\0';
}

void window_draw(void) {
    struct window* w = &active_window;

    draw_rect(w->x, w->y, w->width, 20, palette[COLOR_GRASSGREEN][0], palette[COLOR_GRASSGREEN][1], palette[COLOR_GRASSGREEN][2]);

    // draw the title text inside the bar
    int i = 0;
    int text_x = w->x + 4;
    while (w->title[i]) {
        draw_char(text_x, w->y + 6, w->title[i], palette[COLOR_BLACK][0], palette[COLOR_BLACK][1], palette[COLOR_BLACK][2], 1);
        text_x += 8;
        i++;
    }

    draw_rect(w->x, w->y + 20, w->width, w->height - 20, palette[COLOR_WHITE][0], palette[COLOR_WHITE][1], palette[COLOR_WHITE][2]);

    draw_rect(w->x, w->y, w->width, 2, 0, 0, 0);
    draw_rect(w->x, w->y + w->height - 2, w->width, 2, 0, 0, 0);
    draw_rect(w->x, w->y, 2, w->height, 0, 0, 0);
    draw_rect(w->x + w->width - 2, w->y, 2, w->height, 0, 0, 0);
}
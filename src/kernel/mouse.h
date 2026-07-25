#ifndef MOUSE_H
#define MOUSE_H

struct mouse_state {
    int x, y;
    int left_button;
    int right_button;
};

extern struct mouse_state mouse;

void mouse_install(void);
void mouse_handler_c(void);
void draw_cursor(void);

#endif
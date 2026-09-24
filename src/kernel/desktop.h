#ifndef DESKTOP_H
#define DESKTOP_H

void desktop_init(void);
void desktop_draw(void);
void desktop_handle_key(char c);
void desktop_update(void);
void go_home(void);
void draw_status_bar_thin(void);   // add this line

extern int app_running;

#endif
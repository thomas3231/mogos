#include "splash.h"
#include "graphics.h"
#include "timer.h"
#include "desktop.h"

#define NUM_STREAKS 14

volatile int splash_enter_pressed = 0;

int streak_x[NUM_STREAKS];
int streak_y[NUM_STREAKS];
int streak_speed[NUM_STREAKS];

unsigned int rand_seed = 12345;

int simple_rand(void) {
    rand_seed = rand_seed * 1103515245 + 12345;
    return (rand_seed >> 16) & 0x7FFF;
}

void splash_init_streaks(void) {
    for (int i = 0; i < NUM_STREAKS; i++) {
        streak_x[i] = (simple_rand() % (screen.width / 9)) * 9;
        streak_y[i] = -(simple_rand() % screen.height);
        streak_speed[i] = 1 + (simple_rand() % 3);
    }
}

void splash_draw_background(void) {
    fill_screen(0x00, 0x00, 0x00);

    for (int i = 0; i < NUM_STREAKS; i++) {
        for (int trail = 0; trail < 6; trail++) {
            int y = streak_y[i] - (trail * 10);
            if (y < 0 || y >= screen.height) continue;

            char c = '0' + ((streak_x[i] + y + trail) % 10);
            unsigned char shade = 0xFF - (trail * 35);
            if (shade < 0x20) shade = 0x20;

            draw_char(streak_x[i], y, c, 0x00, shade, 0x00, 1);
        }

        streak_y[i] += streak_speed[i];
        if (streak_y[i] > screen.height + 60) {
            streak_y[i] = -(simple_rand() % 60);
            streak_x[i] = (simple_rand() % (screen.width / 9)) * 9;
            streak_speed[i] = 1 + (simple_rand() % 3);
        }
    }
}

void splash_draw_text(void) {
    char* line1 = "WELCOME USER TO MOGOS";
    char* line2 = "PRESS ENTER TO BEGIN";

    int len1 = 0; while (line1[len1]) len1++;
    int len2 = 0; while (line2[len2]) len2++;

    int y1 = screen.height / 2 - 12;
    int y2 = screen.height / 2 + 4;

    int x1 = (screen.width / 2) - (len1 * 9 / 2);
    int x2 = (screen.width / 2) - (len2 * 9 / 2);

    // dark backing box so text stays readable over the moving streaks
    draw_rect(x1 - 6, y1 - 4, len1 * 9 + 12, 30, 0x00, 0x00, 0x00);

    for (int i = 0; i < len1; i++) {
        draw_char(x1 + i * 9, y1, line1[i], 0xFF, 0xFF, 0xFF, 1);
    }
    for (int i = 0; i < len2; i++) {
        draw_char(x2 + i * 9, y2, line2[i], 0x00, 0xFF, 0x00, 1);
    }
}

void splash_run(void) {
    splash_init_streaks();

    unsigned int last_frame = 0;

    while (1) {
        if (timer_get_ticks() - last_frame >= 5) {   // ~20 fps
            splash_draw_background();
            splash_draw_text();
            screen_swap();
            last_frame = timer_get_ticks();
        }

        __asm__ volatile ("hlt");

        // check for Enter -- see keyboard.c wiring below
        if (splash_enter_pressed) {
            splash_enter_pressed = 0;
            return;
        }
    }
}
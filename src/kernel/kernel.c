#include "graphics.h"
#include "keyboard.h"
#include "timer.h"
#include "heap.h"
#include "palette.h"

extern void keyboard_handler_asm(void);

static void draw_status_bar(void) {
    const int bar_height = 18;
    const int border = 4;
    const unsigned char black_r = palette[COLOR_BLACK][0];
    const unsigned char black_g = palette[COLOR_BLACK][1];
    const unsigned char black_b = palette[COLOR_BLACK][2];
    const unsigned char grass_r = palette[COLOR_GRASSGREEN][0];
    const unsigned char grass_g = palette[COLOR_GRASSGREEN][1];
    const unsigned char grass_b = palette[COLOR_GRASSGREEN][2];

    draw_rect(0, 0, screen.width, bar_height, black_r, black_g, black_b);
    draw_rect(0, screen.height - bar_height, screen.width, bar_height, black_r, black_g, black_b);

    draw_rect(0, screen.height - border, screen.width, border, grass_r, grass_g, grass_b);
    draw_rect(0, 0, border, screen.height, grass_r, grass_g, grass_b);
    draw_rect(screen.width - border, 0, border, screen.height, grass_r, grass_g, grass_b);
}

static void draw_text_line(int x, int y, const char* text) {
    for (int idx = 0; text[idx] != '\0'; idx++) {
        draw_char(x + (idx * 8), y, text[idx], 0xFF, 0xFF, 0xFF, 1);
    }
}

static void clear_status_text_area(void) {
    draw_rect(0, 0, screen.width, 18, 0x00, 0x00, 0x00);
    draw_status_bar();
}

static void draw_status_text(void) {
    unsigned int uptime = timer_get_uptime_seconds();
    unsigned int cpu = timer_get_cpu_usage_percent();
    unsigned int hours = uptime / 3600;
    unsigned int minutes = (uptime / 60) % 60;
    unsigned int seconds = uptime % 60;
    char buffer[32];
    char cpu_buffer[16];
    int pos = 0;

    buffer[pos++] = 'U';
    buffer[pos++] = 'P';
    buffer[pos++] = ':';
    buffer[pos++] = ' ';
    buffer[pos++] = '0' + (hours / 10);
    buffer[pos++] = '0' + (hours % 10);
    buffer[pos++] = 'h';
    buffer[pos++] = ' ';
    buffer[pos++] = '0' + (minutes / 10);
    buffer[pos++] = '0' + (minutes % 10);
    buffer[pos++] = 'm';
    buffer[pos++] = ' ';
    buffer[pos++] = '0' + (seconds / 10);
    buffer[pos++] = '0' + (seconds % 10);
    buffer[pos++] = 's';
    buffer[pos] = '\0';

    clear_status_text_area();
    draw_text_line(8, 4, buffer);

    pos = 0;
    cpu_buffer[pos++] = 'C';
    cpu_buffer[pos++] = 'P';
    cpu_buffer[pos++] = 'U';
    cpu_buffer[pos++] = ':';
    cpu_buffer[pos++] = ' ';
    cpu_buffer[pos++] = '0' + (cpu / 100);
    cpu_buffer[pos++] = '0' + ((cpu / 10) % 10);
    cpu_buffer[pos++] = '0' + (cpu % 10);
    cpu_buffer[pos++] = '%';
    cpu_buffer[pos] = '\0';

    draw_text_line(220, 4, cpu_buffer);
}

static void render_status_frame(void) {
    static unsigned int last_render_tick = 0;
    static unsigned int last_uptime = 0;
    static unsigned int last_cpu = 0;
    unsigned int current_tick = timer_get_ticks();
    unsigned int uptime = timer_get_uptime_seconds();
    unsigned int cpu = timer_get_cpu_usage_percent();

    if (last_render_tick == 0) {
        draw_status_bar();
        draw_status_text();
        last_render_tick = current_tick;
        last_uptime = uptime;
        last_cpu = cpu;
        return;
    }

    if (current_tick - last_render_tick < 100) {
        return;
    }

    if (uptime != last_uptime || cpu != last_cpu) {
        draw_status_text();
        last_uptime = uptime;
        last_cpu = cpu;
    }

    last_render_tick = current_tick;
}

void kernel_main(void) {
    screen_init();
    heap_init();

    fill_screen(0x10, 0x10, 0x10);
    render_status_frame();

    idt_install();
    pic_remap();
    set_idt_gate(33, (unsigned int)keyboard_handler_asm);
    timer_install();
    __asm__ volatile ("sti");

    for (;;) {
        timer_set_idle_state(1);
        __asm__ volatile ("hlt");
        timer_set_idle_state(0);
        render_status_frame();
    }
}
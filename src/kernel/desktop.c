#include "desktop.h"
#include "graphics.h"
#include "palette.h"
#include "timer.h"
#include "rtc.h"
#include "keyboard.h"

int app_running = 0;
int focus = 0;   // 0 = menu, 1 = terminal

void (*current_app_key_handler)(char c) = 0;

#define STATUS_BAR_HEIGHT 10

struct menu_item {
    char label[24];
};

void snake_handle_key(char c) {
    // placeholder -- real Snake logic goes here later
}

void run_snake_stub(void) {
    app_running = 1;
    current_app_key_handler = snake_handle_key;

    fill_screen(0x00, 0x00, 0x00);
    int x = 20;
    char* msg = "SNAKE";
    for (int i = 0; msg[i]; i++) {
        draw_char(x, 20, msg[i], 0x00, 0xFF, 0x00, 2);
        x += 18;
    }
    screen_swap();
}

void about_handle_key(char c) {
    // Ctrl+H (global) is the only way out, nothing to do here
}

void run_about(void) {
    app_running = 1;
    current_app_key_handler = about_handle_key;

    fill_screen(0x00, 0x00, 0x00);

    char* lines[] = {
        "MOGOS - THE LEGEND",
        "",
        "In the year of our LORD,",
        "Thomas Gazan was born into",
        "a world of dial-up modems",
        "and unpatched buffer overflows.",
        "",
        "He toiled for weeks in the",
        "trenches of WSL, wrestling",
        "GDTs and stack offsets by",
        "candlelight, alone, save for",
        "the sacred glow of QEMU.",
        "",
        "Bootloaders crashed. IDTs",
        "misfired. The mouse betrayed",
        "him. Still he pressed on.",
        "",
        "Every line of this OS was",
        "forged by his own two hands,",
        "no shortcuts, no help, just",
        "raw willpower and NASM.",
        "",
        "This is MogOS.",
        "Ctrl+H to return to safety.",
    };
    int line_count = 23;

    int y = STATUS_BAR_HEIGHT + 6;
    for (int i = 0; i < line_count; i++) {
        int x = 4;
        for (int j = 0; lines[i][j]; j++) {
            draw_char(x, y, lines[i][j], 0x00, 0xFF, 0x00, 1);
            x += 9;
        }
        y += 10;
    }

    screen_swap();
}

int setting_toggle_a = 0;
int setting_toggle_b = 1;

void run_settings(void);

void settings_handle_key(char c) {
    if (c == 'a' || c == 'A') {
        setting_toggle_a = !setting_toggle_a;
        run_settings();
    } else if (c == 'b' || c == 'B') {
        setting_toggle_b = !setting_toggle_b;
        run_settings();
    }
}

void run_settings(void) {
    app_running = 1;
    current_app_key_handler = settings_handle_key;

    fill_screen(0x10, 0x10, 0x20);

    char* title = "SETTINGS";
    int x = 4, y = STATUS_BAR_HEIGHT + 6;
    for (int i = 0; title[i]; i++) {
        draw_char(x, y, title[i], 0xFF, 0xFF, 0x00, 1);
        x += 9;
    }
    y += 20;

    char* toggle_a_label = setting_toggle_a ? "[A] Grass green mode: ON" : "[A] Grass green mode: OFF";
    x = 4;
    for (int i = 0; toggle_a_label[i]; i++) {
        draw_char(x, y, toggle_a_label[i], 0xFF, 0xFF, 0xFF, 1);
        x += 9;
    }
    y += 12;

    char* toggle_b_label = setting_toggle_b ? "[B] Boot jingle: ON" : "[B] Boot jingle: OFF";
    x = 4;
    for (int i = 0; toggle_b_label[i]; i++) {
        draw_char(x, y, toggle_b_label[i], 0xFF, 0xFF, 0xFF, 1);
        x += 9;
    }
    y += 20;

    char* hint = "Press A or B to toggle. Ctrl+H to exit.";
    x = 4;
    for (int i = 0; hint[i]; i++) {
        draw_char(x, y, hint[i], 0x88, 0x88, 0x88, 1);
        x += 9;
    }

    screen_swap();
}

struct menu_item menu_items[] = {
    { "Snake" },
    { "Text Editor" },
    { "About" },
    { "Settings" },
    { "Placeholder 5" },
    { "Placeholder 6" },
};
int menu_item_count = 6;
int menu_selected = 0;
int menu_scroll_offset = 0;
#define MENU_VISIBLE_ROWS 8

char terminal_buffer[128];
int terminal_pos = 0;

#define TERMINAL_MAX_LINES 12
char terminal_history[TERMINAL_MAX_LINES][40];
int terminal_line_count = 0;

void draw_status_bar_thin(void) {
    unsigned int uptime = timer_get_uptime_seconds();
    unsigned int cpu = timer_get_cpu_usage_percent();
    unsigned int hours = uptime / 3600;
    unsigned int minutes = (uptime / 60) % 60;
    unsigned int seconds = uptime % 60;

    draw_rect(0, 0, screen.width, STATUS_BAR_HEIGHT, 0x00, 0x00, 0x00);

    char buf[24];
    int p = 0;
    buf[p++] = '0' + (hours / 10);
    buf[p++] = '0' + (hours % 10);
    buf[p++] = ':';
    buf[p++] = '0' + (minutes / 10);
    buf[p++] = '0' + (minutes % 10);
    buf[p++] = ':';
    buf[p++] = '0' + (seconds / 10);
    buf[p++] = '0' + (seconds % 10);
    buf[p] = '\0';

    int cx = 2;
    for (int i = 0; buf[i]; i++) {
        draw_char(cx, 1, buf[i], 0xFF, 0xFF, 0xFF, 1);
        cx += 9;
    }

    char cpu_buf[10];
    p = 0;
    cpu_buf[p++] = 'C';
    cpu_buf[p++] = 'P';
    cpu_buf[p++] = 'U';
    cpu_buf[p++] = ':';
    cpu_buf[p++] = '0' + (cpu / 100);
    cpu_buf[p++] = '0' + ((cpu / 10) % 10);
    cpu_buf[p++] = '0' + (cpu % 10);
    cpu_buf[p++] = '%';
    cpu_buf[p] = '\0';

    int mid_len = 0;
    while (cpu_buf[mid_len]) mid_len++;
    int mx = (screen.width / 2) - (mid_len * 9 / 2);
    for (int i = 0; cpu_buf[i]; i++) {
        draw_char(mx, 1, cpu_buf[i], 0xFF, 0xFF, 0x00, 1);
        mx += 9;
    }

    struct rtc_time now;
    rtc_read(&now);

    char clock_buf[10];
    p = 0;
    clock_buf[p++] = '0' + (now.hours / 10);
    clock_buf[p++] = '0' + (now.hours % 10);
    clock_buf[p++] = ':';
    clock_buf[p++] = '0' + (now.minutes / 10);
    clock_buf[p++] = '0' + (now.minutes % 10);
    clock_buf[p++] = ':';
    clock_buf[p++] = '0' + (now.seconds / 10);
    clock_buf[p++] = '0' + (now.seconds % 10);
    clock_buf[p] = '\0';

    int rx = screen.width - (8 * 9) - 2;
    for (int i = 0; clock_buf[i]; i++) {
        draw_char(rx, 1, clock_buf[i], 0x00, 0xFF, 0xFF, 1);
        rx += 9;
    }
}

void draw_menu(void) {
    if (focus == 0) {
        draw_rect(0, STATUS_BAR_HEIGHT + 1, screen.width / 2 - 1, 1, 0xFF, 0xFF, 0x00);
    }

    int x = 4, y = STATUS_BAR_HEIGHT + 6;
    int shown = 0;
    for (int i = menu_scroll_offset; i < menu_item_count && shown < MENU_VISIBLE_ROWS; i++, shown++) {
        int is_selected = (i == menu_selected);
        unsigned char r = is_selected ? 0xFF : 0xAA;
        unsigned char g = is_selected ? 0xFF : 0xAA;
        unsigned char b = is_selected ? 0x00 : 0xAA;

        if (is_selected) {
            draw_rect(2, y - 1, screen.width / 2 - 6, 9, 0x30, 0x30, 0x00);
        }

        int cx = x;
        for (int j = 0; menu_items[i].label[j]; j++) {
            draw_char(cx, y, menu_items[i].label[j], r, g, b, 1);
            cx += 9;
        }
        y += 10;
    }
}

void draw_terminal(void) {
    if (focus == 1) {
        draw_rect(screen.width / 2, STATUS_BAR_HEIGHT + 1, screen.width / 2, 1, 0xFF, 0xFF, 0x00);
    }

    int tx = screen.width / 2 + 4;
    int ty = STATUS_BAR_HEIGHT + 4;

    for (int i = 0; i < terminal_line_count; i++) {
        int cx = tx;
        for (int j = 0; terminal_history[i][j]; j++) {
            draw_char(cx, ty, terminal_history[i][j], 0xAA, 0xAA, 0xAA, 1);
            cx += 9;
        }
        ty += 10;
    }

    draw_char(tx, ty, '>', 0x00, 0xFF, 0x00, 1);
    int cx = tx + 9;
    for (int i = 0; terminal_buffer[i]; i++) {
        draw_char(cx, ty, terminal_buffer[i], 0xFF, 0xFF, 0xFF, 1);
        cx += 9;
    }
}

void desktop_init(void) {
    app_running = 0;
    current_app_key_handler = 0;
    focus = 0;
    menu_selected = 0;
    menu_scroll_offset = 0;
    terminal_pos = 0;
    terminal_buffer[0] = '\0';
    terminal_line_count = 0;
}

void desktop_draw(void) {
    fill_screen(0x10, 0x10, 0x10);

    draw_status_bar_thin();

    draw_rect(0, STATUS_BAR_HEIGHT, screen.width, 1, 0x7C, 0xFC, 0x00);
    draw_rect(screen.width / 2 - 1, STATUS_BAR_HEIGHT, 1, screen.height - STATUS_BAR_HEIGHT, 0x7C, 0xFC, 0x00);

    draw_menu();
    draw_terminal();

    screen_swap();
}

int str_equals(char* a, char* b) {
    int i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == '\0' && b[i] == '\0';
}

void terminal_print_line(char* text) {
    if (terminal_line_count >= TERMINAL_MAX_LINES) {
        for (int i = 1; i < TERMINAL_MAX_LINES; i++) {
            int j = 0;
            while (terminal_history[i][j]) {
                terminal_history[i - 1][j] = terminal_history[i][j];
                j++;
            }
            terminal_history[i - 1][j] = '\0';
        }
        terminal_line_count = TERMINAL_MAX_LINES - 1;
    }

    int i = 0;
    while (text[i] && i < 39) {
        terminal_history[terminal_line_count][i] = text[i];
        i++;
    }
    terminal_history[terminal_line_count][i] = '\0';
    terminal_line_count++;
}

void shutdown_system(void) {
    outb(0x604, 0x00);
    outw(0x604, 0x2000);

    __asm__ volatile ("cli");
    for (;;) { __asm__ volatile ("hlt"); }
}

void run_terminal_command(char* cmd) {
    char echoed[40];
    echoed[0] = '>';
    int i = 0;
    while (cmd[i] && i < 38) {
        echoed[i + 1] = cmd[i];
        i++;
    }
    echoed[i + 1] = '\0';
    terminal_print_line(echoed);

    if (str_equals(cmd, "ping")) {
        terminal_print_line("pong");
    } else if (str_equals(cmd, "run snake")) {
        run_snake_stub();
    } else if (str_equals(cmd, "about")) {
        run_about();
    } else if (str_equals(cmd, "settings")) {
        run_settings();
    } else if (str_equals(cmd, "exit")) {
        terminal_print_line("shutting down...");
        desktop_draw();
        shutdown_system();
    } else {
        terminal_print_line("unknown command");
    }
}

void desktop_handle_key(char c) {
    if (app_running) {
        if (current_app_key_handler) {
            current_app_key_handler(c);
        }
        return;
    }

    if (c == 3) {   // LEFT arrow
        focus = 0;
        desktop_draw();
        return;
    }
    if (c == 4) {   // RIGHT arrow
        focus = 1;
        desktop_draw();
        return;
    }

    if (focus == 0) {
        if (c == 1) {   // UP
            if (menu_selected > 0) {
                menu_selected--;
                if (menu_selected < menu_scroll_offset) menu_scroll_offset = menu_selected;
            }
            desktop_draw();
        } else if (c == 2) {   // DOWN
            if (menu_selected < menu_item_count - 1) {
                menu_selected++;
                if (menu_selected >= menu_scroll_offset + MENU_VISIBLE_ROWS) {
                    menu_scroll_offset = menu_selected - MENU_VISIBLE_ROWS + 1;
                }
            }
            desktop_draw();
        } else if (c == '\n') {
            if (str_equals(menu_items[menu_selected].label, "Snake")) {
                run_snake_stub();
            } else if (str_equals(menu_items[menu_selected].label, "About")) {
                run_about();
            } else if (str_equals(menu_items[menu_selected].label, "Settings")) {
                run_settings();
            }
        }
    } else {
        if (c == '\n') {
            run_terminal_command(terminal_buffer);
            terminal_pos = 0;
            terminal_buffer[0] = '\0';
            desktop_draw();
        } else if (c == '\b') {
            if (terminal_pos > 0) {
                terminal_pos--;
                terminal_buffer[terminal_pos] = '\0';
            }
            desktop_draw();
        } else if (c >= 32 && c < 127) {
            if (terminal_pos < 127) {
                terminal_buffer[terminal_pos++] = c;
                terminal_buffer[terminal_pos] = '\0';
            }
            desktop_draw();
        }
    }
}

void go_home(void) {
    app_running = 0;
    current_app_key_handler = 0;
    desktop_draw();
}
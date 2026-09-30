#include "desktop.h"
#include "graphics.h"
#include "palette.h"
#include "timer.h"
#include "rtc.h"
#include "keyboard.h"
#include "snake.h"



int app_running = 0;
int focus = 0;   // 0 = menu, 1 = terminal
unsigned int desktop_last_draw_tick = 0;

void (*current_app_key_handler)(char c) = 0;

#define STATUS_BAR_HEIGHT 10

struct menu_item {
    char label[24];
};

void run_snake(void) {
    app_running = 1;
    current_app_key_handler = snake_key_handler;
    snake_start();
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
        "misfired. Still he pressed on.",
        "",
        "Every line of this OS was",
        "forged by his own two hands,",
        "no shortcuts, no help, just",
        "raw willpower and NASM.",
        "",
        "This is MogOS.",
        "Ctrl+H to return to safety.",
        "This was AI generated btw",
    };
    int line_count = sizeof(lines) / sizeof(lines[0]);

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

#define EDITOR_CAPACITY 2048
#define EDITOR_CONTENT_X 4
#define EDITOR_CONTENT_Y (STATUS_BAR_HEIGHT + 16)
#define EDITOR_CHAR_WIDTH 9
#define EDITOR_LINE_HEIGHT 10

char editor_buffer[EDITOR_CAPACITY];
int editor_length = 0;
int editor_cursor = 0;
int editor_scroll_row = 0;

void editor_position_to_visual(int position, int columns, int* row, int* column) {
    *row = 0;
    *column = 0;

    for (int i = 0; i < position; i++) {
        if (editor_buffer[i] == '\n') {
            (*row)++;
            *column = 0;
        } else {
            (*column)++;
            if (*column >= columns) {
                (*row)++;
                *column = 0;
            }
        }
    }
}

int editor_vertical_target(int target_row, int target_column, int columns) {
    int best_position = editor_cursor;
    int best_distance = columns + 1;
    int found = 0;

    for (int position = 0; position <= editor_length; position++) {
        int row, column;
        editor_position_to_visual(position, columns, &row, &column);
        if (row != target_row) continue;

        int distance = column > target_column ? column - target_column : target_column - column;
        if (!found || distance < best_distance) {
            best_position = position;
            best_distance = distance;
            found = 1;
        }
    }

    return best_position;
}

void editor_draw(void) {
    fill_screen(0x10, 0x10, 0x10);
    draw_rect(0, 0, screen.width, STATUS_BAR_HEIGHT, 0x00, 0x00, 0x00);

    char* heading = "TEXT EDITOR  ENTER:new line  arrows:move  BACKSPACE:delete  CTRL+H:exit";
    for (int i = 0; heading[i]; i++) {
        draw_char(4 + i * EDITOR_CHAR_WIDTH, 1, heading[i], 0xFF, 0xFF, 0xFF, 1);
    }

    int columns = (screen.width - EDITOR_CONTENT_X * 2) / EDITOR_CHAR_WIDTH;
    int visible_rows = (screen.height - EDITOR_CONTENT_Y) / EDITOR_LINE_HEIGHT;
    if (columns < 1 || visible_rows < 1) {
        screen_swap();
        return;
    }

    int cursor_row, cursor_column;
    editor_position_to_visual(editor_cursor, columns, &cursor_row, &cursor_column);
    if (cursor_row < editor_scroll_row) editor_scroll_row = cursor_row;
    if (cursor_row >= editor_scroll_row + visible_rows) {
        editor_scroll_row = cursor_row - visible_rows + 1;
    }

    int row = 0;
    int column = 0;
    for (int i = 0; i <= editor_length; i++) {
        if (i == editor_cursor && row >= editor_scroll_row && row < editor_scroll_row + visible_rows) {
            int caret_x = EDITOR_CONTENT_X + column * EDITOR_CHAR_WIDTH;
            int caret_y = EDITOR_CONTENT_Y + (row - editor_scroll_row) * EDITOR_LINE_HEIGHT;
            draw_rect(caret_x, caret_y, 2, 8, 0x00, 0xFF, 0x70);
        }

        if (i == editor_length) break;

        if (editor_buffer[i] == '\n') {
            row++;
            column = 0;
        } else {
            if (row >= editor_scroll_row && row < editor_scroll_row + visible_rows) {
                int x = EDITOR_CONTENT_X + column * EDITOR_CHAR_WIDTH;
                int y = EDITOR_CONTENT_Y + (row - editor_scroll_row) * EDITOR_LINE_HEIGHT;
                draw_char(x, y, editor_buffer[i], 0xE0, 0xE0, 0xE0, 1);
            }
            column++;
            if (column >= columns) {
                row++;
                column = 0;
            }
        }
    }

    screen_swap();
}

void editor_handle_key(char c) {
    int columns = (screen.width - EDITOR_CONTENT_X * 2) / EDITOR_CHAR_WIDTH;
    int cursor_row, cursor_column;
    editor_position_to_visual(editor_cursor, columns, &cursor_row, &cursor_column);

    if (c == 3 && editor_cursor > 0) {
        editor_cursor--;
    } else if (c == 4 && editor_cursor < editor_length) {
        editor_cursor++;
    } else if (c == 1 && cursor_row > 0) {
        editor_cursor = editor_vertical_target(cursor_row - 1, cursor_column, columns);
    } else if (c == 2) {
        editor_cursor = editor_vertical_target(cursor_row + 1, cursor_column, columns);
    } else if (c == '\b' && editor_cursor > 0) {
        for (int i = editor_cursor - 1; i < editor_length; i++) {
            editor_buffer[i] = editor_buffer[i + 1];
        }
        editor_cursor--;
        editor_length--;
    } else if ((c == '\n' || (c >= 32 && c < 127)) && editor_length < EDITOR_CAPACITY - 1) {
        for (int i = editor_length; i > editor_cursor; i--) {
            editor_buffer[i] = editor_buffer[i - 1];
        }
        editor_buffer[editor_cursor++] = c;
        editor_length++;
        editor_buffer[editor_length] = '\0';
    }

    editor_draw();
}

void run_text_editor(void) {
    app_running = 1;
    current_app_key_handler = editor_handle_key;
    editor_length = 0;
    editor_cursor = 0;
    editor_scroll_row = 0;
    editor_buffer[0] = '\0';
    editor_draw();
}

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
    desktop_last_draw_tick = timer_get_ticks();
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
        run_snake();
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
                run_snake();
            } else if (str_equals(menu_items[menu_selected].label, "Text Editor")) {
                run_text_editor();
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
            if (!app_running) desktop_draw();
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

void desktop_update(void) {
    if (app_running && current_app_key_handler == snake_key_handler) {
        snake_update();
    } else if (!app_running && timer_get_ticks() - desktop_last_draw_tick >= 100) {
        desktop_draw();
    }
}

void go_home(void) {
    app_running = 0;
    current_app_key_handler = 0;
    desktop_draw();
}
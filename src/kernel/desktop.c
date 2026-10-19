#include "desktop.h"
#include "graphics.h"
#include "palette.h"
#include "timer.h"
#include "rtc.h"
#include "keyboard.h"
#include "snake.h"
#include "ata.h"
#include "speaker.h"



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
    (void)c;
}

void run_about(void) {
    app_running = 1;
    current_app_key_handler = about_handle_key;

    fill_screen(0x00, 0x00, 0x00);

    char* lines[] = {
        "MOGOS - THE LEGEND RETURNS",
        "",
        "In the beginning, there was BIOS.",
        "Then came MogOS. The fans got louder.",
        "",
        "It stared into the void of Ring 0",
        "and the void forgot to set a stack.",
        "",
        "The keyboard spoke. The timer ticked.",
        "Snake ate. The pixels obeyed.",
        "",
        "Then the editor asked to save.",
        "\"Where?\" asked the read-only ISO.",
        "A writable disk joined the party.",
        "Five sectors later: history.",
        "",
        "No filesystem. No mouse. No network.",
        "Just one document and unreasonable",
        "confidence in a 32-bit kernel.",
        "",
        "Ctrl+H: return to the desktop.",
        "Please clap; the IRQs are listening.",
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

int editor_green_text = 0;
int clock_show_seconds = 1;
volatile int boot_jingle_pending = 0;

void run_settings(void);

void settings_handle_key(char c) {
    if (c == 'a' || c == 'A') {
        editor_green_text = !editor_green_text;
        run_settings();
    } else if (c == 'b' || c == 'B') {
        boot_jingle_pending = 1;
        run_settings();
    } else if (c == 'c' || c == 'C') {
        clock_show_seconds = !clock_show_seconds;
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

    char* toggle_a_label = editor_green_text ? "[A] Green editor text: ON" : "[A] Green editor text: OFF";
    x = 4;
    for (int i = 0; toggle_a_label[i]; i++) {
        draw_char(x, y, toggle_a_label[i], 0xFF, 0xFF, 0xFF, 1);
        x += 9;
    }
    y += 12;

    char* toggle_b_label = "[B] Play jingle now";
    x = 4;
    for (int i = 0; toggle_b_label[i]; i++) {
        draw_char(x, y, toggle_b_label[i], 0xFF, 0xFF, 0xFF, 1);
        x += 9;
    }
    y += 12;

    char* toggle_c_label = clock_show_seconds ? "[C] Clock seconds: ON" : "[C] Clock seconds: OFF";
    x = 4;
    for (int i = 0; toggle_c_label[i]; i++) {
        draw_char(x, y, toggle_c_label[i], 0xFF, 0xFF, 0xFF, 1);
        x += 9;
    }
    y += 20;

    char* hint = "A/C toggle. B plays jingle. Ctrl+H exits.";
    x = 4;
    for (int i = 0; hint[i]; i++) {
        draw_char(x, y, hint[i], 0x88, 0x88, 0x88, 1);
        x += 9;
    }
    y += 12;
    char* session_hint = "Settings reset when MogOS restarts.";
    x = 4;
    for (int i = 0; session_hint[i]; i++) {
        draw_char(x, y, session_hint[i], 0x88, 0x88, 0x88, 1);
        x += 9;
    }

    screen_swap();
}

struct menu_item menu_items[] = {
    { "Snake" },
    { "Text Editor" },
    { "About" },
    { "Settings" },
    { "Exit" },
};
int menu_item_count = 5;
int menu_selected = 0;
int menu_scroll_offset = 0;
#define MENU_VISIBLE_ROWS 8

char terminal_buffer[128];
int terminal_pos = 0;

#define TERMINAL_MAX_LINES 12
char terminal_history[TERMINAL_MAX_LINES][40];
int terminal_line_count = 0;
static int terminal_fetch_visible = 0;

#define EDITOR_CAPACITY 2043
#define EDITOR_CONTENT_X 4
#define EDITOR_CONTENT_Y (STATUS_BAR_HEIGHT + 16)
#define EDITOR_CHAR_WIDTH 9
#define EDITOR_LINE_HEIGHT 10
#define EDITOR_SAVE_LBA 1
#define EDITOR_SAVE_SECTORS 5
#define EDITOR_SAVE_BYTES (EDITOR_SAVE_SECTORS * 512)
#define EDITOR_SAVE_HEADER 6

char editor_buffer[EDITOR_CAPACITY];
unsigned char editor_save_image[EDITOR_SAVE_BYTES];
char editor_status[48];
int editor_length = 0;
int editor_cursor = 0;
int editor_scroll_row = 0;

void editor_set_status(const char* message) {
    int i = 0;
    while (message[i] && i < (int)sizeof(editor_status) - 1) {
        editor_status[i] = message[i];
        i++;
    }
    editor_status[i] = '\0';
}

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
    if (columns < 1) return editor_cursor;

    int best_position = editor_cursor;
    int best_distance = columns + 1;
    int found = 0;
    int row = 0;
    int column = 0;

    for (int position = 0; position <= editor_length; position++) {
        if (row == target_row) {
            int distance = column > target_column ? column - target_column : target_column - column;
            if (!found || distance < best_distance) {
                best_position = position;
                best_distance = distance;
                found = 1;
            }
        }

        if (position == editor_length) break;
        if (editor_buffer[position] == '\n') {
            row++;
            column = 0;
        } else if (++column >= columns) {
            row++;
            column = 0;
        }
    }

    return best_position;
}

void editor_draw(void) {
    fill_screen(0x10, 0x10, 0x10);
    draw_rect(0, 0, screen.width, STATUS_BAR_HEIGHT, 0x00, 0x00, 0x00);

    char* heading = "EDITOR  Ctrl+S/F2:save Ctrl+O/F3:load Enter:new line arrows:move Ctrl+H:exit";
    for (int i = 0; heading[i]; i++) {
        draw_char(4 + i * EDITOR_CHAR_WIDTH, 1, heading[i], 0xFF, 0xFF, 0xFF, 1);
    }
    for (int i = 0; editor_status[i]; i++) {
        draw_char(4 + i * EDITOR_CHAR_WIDTH, 12, editor_status[i], 0x00, 0xD0, 0xD0, 1);
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
                draw_char(x, y, editor_buffer[i], editor_green_text ? 0x00 : 0xE0,
                          editor_green_text ? 0xFF : 0xE0, editor_green_text ? 0x70 : 0xE0, 1);
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

int editor_save_document(void) {
    for (int i = 0; i < EDITOR_SAVE_BYTES; i++) editor_save_image[i] = 0;
    editor_save_image[0] = 'M';
    editor_save_image[1] = 'O';
    editor_save_image[2] = 'G';
    editor_save_image[3] = '1';
    editor_save_image[4] = editor_length & 0xFF;
    editor_save_image[5] = (editor_length >> 8) & 0xFF;
    for (int i = 0; i < editor_length; i++) {
        editor_save_image[EDITOR_SAVE_HEADER + i] = editor_buffer[i];
    }

    for (int sector = 0; sector < EDITOR_SAVE_SECTORS; sector++) {
        if (!ata_write_sector(EDITOR_SAVE_LBA + sector,
                              &editor_save_image[sector * 512])) {
            return 0;
        }
    }

    for (int sector = 0; sector < EDITOR_SAVE_SECTORS; sector++) {
        unsigned char verify[512];
        if (!ata_read_sector(EDITOR_SAVE_LBA + sector, verify)) {
            return 0;
        }
        for (int i = 0; i < 512; i++) {
            if (verify[i] != editor_save_image[sector * 512 + i]) {
                return 0;
            }
        }
    }
    return 1;
}

int editor_load_document(void) {
    for (int sector = 0; sector < EDITOR_SAVE_SECTORS; sector++) {
        if (!ata_read_sector(EDITOR_SAVE_LBA + sector,
                             &editor_save_image[sector * 512])) {
            return -1;
        }
    }

    if (editor_save_image[0] != 'M' || editor_save_image[1] != 'O' ||
        editor_save_image[2] != 'G' || editor_save_image[3] != '1') {
        return 0;
    }

    int length = editor_save_image[4] | (editor_save_image[5] << 8);
    if (length >= EDITOR_CAPACITY || EDITOR_SAVE_HEADER + length > EDITOR_SAVE_BYTES) {
        return -2;
    }

    for (int i = 0; i < length; i++) {
        editor_buffer[i] = editor_save_image[EDITOR_SAVE_HEADER + i];
    }
    editor_length = length;
    editor_cursor = length;
    editor_buffer[editor_length] = '\0';
    editor_scroll_row = 0;
    return 1;
}

void editor_handle_key(char c) {
    if (c == 19) {
        editor_set_status(editor_save_document() ? "Saved to attached ATA disk" :
                          "Save failed: disk missing or I/O error");
        editor_draw();
        return;
    }
    if (c == 15) {
        int result = editor_load_document();
        editor_set_status(result > 0 ? "Loaded saved document" :
                          result == 0 ? "No saved document found" :
                          result == -2 ? "Saved document is corrupt; editor unchanged" :
                          "Disk read failed; editor unchanged");
        editor_draw();
        return;
    }

    int columns = (screen.width - EDITOR_CONTENT_X * 2) / EDITOR_CHAR_WIDTH;
    int cursor_row, cursor_column;
    editor_position_to_visual(editor_cursor, columns, &cursor_row, &cursor_column);
    int changed = 0;

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
        changed = 1;
    } else if (c == '\n' || (c >= 32 && c < 127)) {
        if (editor_length < EDITOR_CAPACITY - 1) {
            for (int i = editor_length; i > editor_cursor; i--) {
                editor_buffer[i] = editor_buffer[i - 1];
            }
            editor_buffer[editor_cursor++] = c;
            editor_length++;
            editor_buffer[editor_length] = '\0';
            changed = 1;
        } else {
            editor_set_status("Document full; delete text to continue");
        }
    }

    if (changed) editor_set_status("Unsaved changes");
    editor_draw();
}

void run_text_editor(void) {
    app_running = 1;
    current_app_key_handler = editor_handle_key;
    editor_length = 0;
    editor_cursor = 0;
    editor_scroll_row = 0;
    editor_buffer[0] = '\0';
    int result = editor_load_document();
    editor_set_status(result > 0 ? "Loaded saved document" :
                      result == 0 ? "New document; Ctrl+S to save" :
                      result == -2 ? "Saved document is corrupt; starting empty" :
                      "Cannot read save slot; edits are temporary");
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
    int rtc_valid = rtc_read(&now);

    char clock_buf[10];
    p = 0;
    if (rtc_valid) {
        clock_buf[p++] = '0' + (now.hours / 10);
        clock_buf[p++] = '0' + (now.hours % 10);
        clock_buf[p++] = ':';
        clock_buf[p++] = '0' + (now.minutes / 10);
        clock_buf[p++] = '0' + (now.minutes % 10);
    } else {
        clock_buf[p++] = '-';
        clock_buf[p++] = '-';
        clock_buf[p++] = ':';
        clock_buf[p++] = '-';
        clock_buf[p++] = '-';
    }
    if (clock_show_seconds) {
        clock_buf[p++] = ':';
        clock_buf[p++] = rtc_valid ? '0' + (now.seconds / 10) : '-';
        clock_buf[p++] = rtc_valid ? '0' + (now.seconds % 10) : '-';
    }
    clock_buf[p] = '\0';

    int clock_length = 0;
    while (clock_buf[clock_length]) clock_length++;
    int rx = screen.width - (clock_length * 9) - 2;
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

static int fetch_format_unsigned(char* buffer, unsigned int value) {
    char digits[10];
    int digit_count = 0;
    do {
        digits[digit_count++] = '0' + (value % 10);
        value /= 10;
    } while (value != 0);

    int length = 0;
    while (digit_count > 0) buffer[length++] = digits[--digit_count];
    buffer[length] = '\0';
    return length;
}

static void fetch_draw_text(int x, int y, const char* text,
                            unsigned char r, unsigned char g, unsigned char b) {
    for (int i = 0; text[i]; i++) {
        draw_char(x + i * 9, y, text[i], r, g, b, 1);
    }
}

static void fetch_draw_field(int x, int y, const char* label, const char* value,
                             unsigned char label_r, unsigned char label_g, unsigned char label_b,
                             unsigned char value_r, unsigned char value_g, unsigned char value_b) {
    int label_length = 0;
    while (label[label_length]) {
        draw_char(x + label_length * 9, y, label[label_length],
                  label_r, label_g, label_b, 1);
        label_length++;
    }
    fetch_draw_text(x + label_length * 9, y, value, value_r, value_g, value_b);
}

void draw_terminal(void) {
    if (focus == 1) {
        draw_rect(screen.width / 2, STATUS_BAR_HEIGHT + 1, screen.width / 2, 1, 0xFF, 0xFF, 0x00);
    }

    int tx = screen.width / 2 + 4;
    int ty = STATUS_BAR_HEIGHT + 4;

    if (terminal_fetch_visible) {
        int info_x = tx;
        const unsigned char accent_r = 0xE0, accent_g = 0x70, accent_b = 0xA0;
        const unsigned char value_r = 0xD0, value_g = 0xD0, value_b = 0xD0;
        fetch_draw_text(info_x, ty, "Thomas@MogOS", accent_r, accent_g, accent_b);

        int info_y = ty + 16;
        for (int i = 0; i < 12; i++) {
            draw_char(info_x + i * 9, info_y, '-', accent_r, accent_g, accent_b, 1);
        }
        info_y += 12;

        char uptime[16];
        char cpu[8];
        char display[16];
        int pos = fetch_format_unsigned(uptime, timer_get_uptime_seconds());
        uptime[pos++] = ' ';
        uptime[pos++] = 's';
        uptime[pos] = '\0';

        pos = fetch_format_unsigned(cpu, timer_get_cpu_usage_percent());
        cpu[pos++] = '%';
        cpu[pos] = '\0';

        pos = fetch_format_unsigned(display, screen.width);
        display[pos++] = 'x';
        pos += fetch_format_unsigned(display + pos, screen.height);
        display[pos] = '\0';

        const char* labels[] = {
            "OS:", "Kernel:", "Shell:", "Uptime:",
            "CPU:", "Display:", "Apps:"
        };
        const char* values[] = {
            "MogOS 32-bit", "Ring 0", "MogOS terminal", uptime,
            cpu, display, "Snake, Editor, About, Settings"
        };
        for (int i = 0; i < 7; i++) {
            fetch_draw_field(info_x, info_y, labels[i], values[i],
                             accent_r, accent_g, accent_b, value_r, value_g, value_b);
            info_y += 11;
        }
        ty = info_y + 4;
    } else {
        for (int i = 0; i < terminal_line_count; i++) {
            int cx = tx;
            for (int j = 0; terminal_history[i][j]; j++) {
                draw_char(cx, ty, terminal_history[i][j], 0xAA, 0xAA, 0xAA, 1);
                cx += 9;
            }
            ty += 10;
        }
    }

    draw_char(tx, ty, '>', 0x00, 0xFF, 0x00, 1);
    int visible_chars = (screen.width - tx - 4) / 9 - 1;
    if (visible_chars < 0) visible_chars = 0;
    int first_visible = terminal_pos > visible_chars ? terminal_pos - visible_chars : 0;
    int cx = tx + 9;
    for (int i = first_visible; i < terminal_pos; i++) {
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
    terminal_fetch_visible = 1;
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
    int is_fetch = str_equals(cmd, "fetch");
    if (is_fetch) {
        terminal_line_count = 0;
    } else {
        terminal_fetch_visible = 0;
        char echoed[40];
        echoed[0] = '>';
        int i = 0;
        while (cmd[i] && i < 38) {
            echoed[i + 1] = cmd[i];
            i++;
        }
        echoed[i + 1] = '\0';
        terminal_print_line(echoed);
    }

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
    } else if (str_equals(cmd, "clear")) {
        terminal_line_count = 0;
    }else if (str_equals(cmd, "help")) {
        terminal_print_line("Available commands:");
        terminal_print_line("ping - test command");
        terminal_print_line("run snake - launch Snake game");
        terminal_print_line("about - show about screen");
        terminal_print_line("settings - open settings menu");
        terminal_print_line("fetch - show MogOS system info");
        terminal_print_line("exit - shut down the system");
        terminal_print_line("clear - clear the terminal history");
        terminal_print_line("run editor - launch the text editor");
        terminal_print_line("help - show this help message");
    }else if (str_equals(cmd, "run editor")) {
        run_text_editor();
    }else if (str_equals(cmd, "play jingle")) {
        boot_jingle_pending = 1;
    }else if (is_fetch) {
        terminal_fetch_visible = 1;
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
            } else if (str_equals(menu_items[menu_selected].label, "Exit")) {
                shutdown_system();
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
    if (boot_jingle_pending) {
        boot_jingle_pending = 0;
        play_boot_jingle();
    }

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
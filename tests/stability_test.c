#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "graphics.h"
#include "rtc.h"
#include "timer.h"
#include "snake.h"
#include "ata.h"

struct screen_info screen;

static unsigned char cmos[128];
static unsigned char cmos_register;
static int cmos_update_stuck;
static int ata_selected;
static int ata_present = 1;
static unsigned char ata_status;
static unsigned int ata_data_words;
static unsigned int ata_write_words;

extern volatile unsigned int tick_count;
extern volatile unsigned int uptime_seconds;
extern volatile unsigned int idle_ticks;
extern volatile unsigned int busy_ticks;
extern volatile unsigned int cpu_usage_percent;
extern volatile unsigned int idle_state;

extern int snake_direction_x;
extern int snake_direction_y;
extern int snake_next_direction_x;
extern int snake_next_direction_y;
extern int snake_length;
extern int snake_board_width;
extern int snake_board_height;
extern int snake_game_over;
extern int snake_won;

struct test_snake_segment {
    int x;
    int y;
};

extern struct test_snake_segment snake_body[128];
extern void snake_place_food(void);

extern char editor_buffer[2043];
extern char editor_status[48];
extern int editor_length;
extern int editor_cursor;
extern void editor_handle_key(char key);
extern int editor_load_document(void);

void outb(unsigned short port, unsigned char value) {
    if (port == 0x70) cmos_register = value & 0x7F;
    if (port == 0x1F6) {
        ata_selected = 1;
        ata_status = ata_present ? 0x50 : 0;
        ata_data_words = 0;
    }
    if (port == 0x1F7 && ata_selected) {
        ata_data_words = 0;
        ata_status = ata_present ?
                     value == 0x20 || value == 0x30 ? 0x58 : 0x50 : 0;
    }
}

unsigned char inb(unsigned short port) {
    if (port == 0x1F7 || port == 0x3F6) return ata_status;
    if (port != 0x71) return 0;
    if (cmos_register == 0x0A && cmos_update_stuck) return 0x80;
    return cmos[cmos_register];
}

void outw(unsigned short port, unsigned short value) {
    (void)value;
    if (port == 0x1F0 && ata_status == 0x58) {
        ata_write_words++;
        if (++ata_data_words == 256) ata_status = 0x50;
    }
}

unsigned short inw(unsigned short port) {
    if (port == 0x1F0 && ata_status == 0x58) {
        if (++ata_data_words == 256) ata_status = 0x50;
        return 0;
    }
    return 0;
}

void set_idt_gate(int n, unsigned int handler) {
    (void)n;
    (void)handler;
}

void timer_handler_asm(void) {}

void fill_screen(unsigned char r, unsigned char g, unsigned char b) {
    (void)r; (void)g; (void)b;
}

void draw_rect(int x, int y, int width, int height,
               unsigned char r, unsigned char g, unsigned char b) {
    (void)x; (void)y; (void)width; (void)height;
    (void)r; (void)g; (void)b;
}

void draw_char(int x, int y, char c, unsigned char r,
               unsigned char g, unsigned char b, int scale) {
    (void)x; (void)y; (void)c; (void)r; (void)g; (void)b; (void)scale;
}

void screen_swap(void) {}

static void set_rtc(unsigned char status_b, unsigned char second,
                    unsigned char minute, unsigned char hour,
                    unsigned char day, unsigned char month, unsigned char year) {
    memset(cmos, 0, sizeof(cmos));
    cmos[0x0B] = status_b;
    cmos[0x00] = second;
    cmos[0x02] = minute;
    cmos[0x04] = hour;
    cmos[0x07] = day;
    cmos[0x08] = month;
    cmos[0x09] = year;
    cmos_update_stuck = 0;
}

static void test_rtc_formats_and_failure(void) {
    struct rtc_time value;

    set_rtc(0x02, 0x58, 0x59, 0x23, 0x14, 0x10, 0x26);
    assert(rtc_read(&value));
    assert(value.seconds == 58 && value.minutes == 59 && value.hours == 23);
    assert(value.day == 14 && value.month == 10 && value.year == 26);

    set_rtc(0x06, 58, 59, 23, 14, 10, 26);
    assert(rtc_read(&value));
    assert(value.seconds == 58 && value.minutes == 59 && value.hours == 23);

    set_rtc(0x00, 0x00, 0x01, 0x12, 0x14, 0x10, 0x26);
    assert(rtc_read(&value) && value.hours == 0);
    cmos[0x04] = 0x91;
    assert(rtc_read(&value) && value.hours == 23);

    cmos[0x02] = 0x6A;
    value.minutes = 99;
    assert(!rtc_read(&value));
    assert(value.seconds == 0 && value.minutes == 0 && value.hours == 0);

    set_rtc(0x02, 0, 0, 0, 1, 1, 26);
    cmos_update_stuck = 1;
    assert(!rtc_read(&value));
    assert(value.day == 0 && value.month == 0 && value.year == 0);
    assert(!rtc_read(0));
}

static void test_timer_counters(void) {
    tick_count = uptime_seconds = idle_ticks = busy_ticks = cpu_usage_percent = 0;
    for (unsigned int i = 0; i < 100; i++) {
        timer_set_idle_state(i < 50);
        timer_handler_c();
    }
    assert(timer_get_ticks() == 100);
    assert(timer_get_uptime_seconds() == 1);
    assert(timer_get_cpu_usage_percent() == 50);

    tick_count = 0xFFFFFFCEU;
    uptime_seconds = idle_ticks = busy_ticks = cpu_usage_percent = 0;
    for (unsigned int i = 0; i < 100; i++) {
        timer_set_idle_state(i < 50);
        timer_handler_c();
    }
    assert(timer_get_ticks() == 50);
    assert(timer_get_uptime_seconds() == 1);
    assert(timer_get_cpu_usage_percent() == 50);
}

static void test_snake_queued_turns(void) {
    screen.width = 320;
    screen.height = 240;
    tick_count = 0;
    snake_start();

    snake_key_handler('w');
    assert(snake_next_direction_x == 0 && snake_next_direction_y == -1);

    snake_key_handler('a');
    assert(snake_next_direction_x == 0 && snake_next_direction_y == -1);

    snake_key_handler('s');
    assert(snake_next_direction_x == 0 && snake_next_direction_y == -1);

    snake_key_handler('d');
    assert(snake_next_direction_x == 1 && snake_next_direction_y == 0);
}

static void test_snake_full_board_finishes_without_hanging(void) {
    snake_board_width = 2;
    snake_board_height = 2;
    snake_length = 4;
    snake_game_over = 0;
    snake_won = 0;
    snake_body[0] = (struct test_snake_segment){0, 0};
    snake_body[1] = (struct test_snake_segment){1, 0};
    snake_body[2] = (struct test_snake_segment){0, 1};
    snake_body[3] = (struct test_snake_segment){1, 1};

    snake_place_food();
    assert(snake_game_over && snake_won);
}

static void test_editor_size_limit_and_unsaved_status(void) {
    screen.width = 1024;
    screen.height = 768;
    editor_length = 2042;
    editor_cursor = editor_length;
    memset(editor_buffer, 'x', editor_length);
    editor_buffer[editor_length] = '\0';

    editor_handle_key('y');
    assert(editor_length == 2042 && editor_cursor == 2042);
    assert(editor_buffer[2041] == 'x' && editor_buffer[2042] == '\0');
    assert(strcmp(editor_status, "Document full; delete text to continue") == 0);

    editor_handle_key('\b');
    assert(editor_length == 2041 && editor_cursor == 2041);
    assert(strcmp(editor_status, "Unsaved changes") == 0);
}

static void test_editor_vertical_navigation(void) {
    const char text[] = "one\ntwo\nthree";
    memcpy(editor_buffer, text, sizeof(text));
    editor_length = (int)sizeof(text) - 1;
    editor_cursor = 1;

    editor_handle_key(2);
    assert(editor_cursor == 5);
    editor_handle_key(2);
    assert(editor_cursor == 9);
    editor_handle_key(1);
    assert(editor_cursor == 5);

    int position = 0;
    for (int line = 0; line < 80; line++) {
        for (int column = 0; column < 24; column++) {
            editor_buffer[position++] = 'a';
        }
        if (line < 79) editor_buffer[position++] = '\n';
    }
    editor_buffer[position] = '\0';
    editor_length = position;
    editor_cursor = 10 * 25 + 17;
    for (int line = 0; line < 50; line++) editor_handle_key(2);
    assert(editor_cursor == 60 * 25 + 17);
    for (int line = 0; line < 100; line++) editor_handle_key(2);
    assert(editor_cursor == 79 * 25 + 17);
}

static void test_editor_empty_save_slot(void) {
    editor_buffer[0] = '\0';
    editor_length = 0;
    editor_cursor = 0;
    assert(editor_load_document() == 0);
    assert(editor_length == 0 && editor_cursor == 0);
}

static void test_editor_disk_failures_preserve_document(void) {
    memcpy(editor_buffer, "keep", 5);
    editor_length = 4;
    editor_cursor = 4;
    ata_present = 0;

    assert(editor_load_document() == -1);
    assert(editor_length == 4 && editor_cursor == 4);
    assert(strcmp(editor_buffer, "keep") == 0);

    editor_handle_key(15);
    assert(strcmp(editor_status, "Disk read failed; editor unchanged") == 0);
    assert(editor_length == 4 && strcmp(editor_buffer, "keep") == 0);

    editor_handle_key(19);
    assert(strcmp(editor_status, "Save failed: disk missing or I/O error") == 0);
    ata_present = 1;
}

static void test_ata_selects_device_before_polling(void) {
    unsigned char sector[512] = {0};
    ata_selected = 0;
    ata_status = 0;
    ata_data_words = 0;
    ata_write_words = 0;

    assert(ata_write_sector(1, sector));
    assert(ata_selected);
    assert(ata_write_words == 256);
}

int main(void) {
    test_rtc_formats_and_failure();
    test_timer_counters();
    test_snake_queued_turns();
    test_snake_full_board_finishes_without_hanging();
    test_editor_size_limit_and_unsaved_status();
    test_editor_vertical_navigation();
    test_editor_empty_save_slot();
    test_editor_disk_failures_preserve_document();
    test_ata_selects_device_before_polling();
    puts("MogOS stability tests passed");
    return 0;
}

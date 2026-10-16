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

void outb(unsigned short port, unsigned char value) {
    if (port == 0x70) cmos_register = value & 0x7F;
    if (port == 0x1F6) {
        ata_selected = 1;
        ata_status = 0x50;
        ata_data_words = 0;
    }
    if (port == 0x1F7 && ata_selected) {
        ata_data_words = 0;
        ata_status = value == 0x20 || value == 0x30 ? 0x58 : 0x50;
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
    test_ata_selects_device_before_polling();
    puts("MogOS stability tests passed");
    return 0;
}

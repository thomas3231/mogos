#include "rtc.h"
#include "keyboard.h"   // reuses outb/inb

#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71
#define RTC_READ_RETRIES 100000U

unsigned char cmos_read(unsigned char reg) {
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

int rtc_update_in_progress(void) {
    outb(CMOS_ADDRESS, 0x0A);
    return inb(CMOS_DATA) & 0x80;
}

unsigned char bcd_to_bin(unsigned char val) {
    return (val & 0x0F) + ((val / 16) * 10);
}

int rtc_read(struct rtc_time* out) {
    if (!out) return 0;
    out->seconds = 0;
    out->minutes = 0;
    out->hours = 0;
    out->day = 0;
    out->month = 0;
    out->year = 0;

    for (unsigned int attempt = 0; attempt < RTC_READ_RETRIES; attempt++) {
        if (rtc_update_in_progress()) continue;

        unsigned char status_b = cmos_read(0x0B);
        unsigned char second = cmos_read(0x00);
        unsigned char minute = cmos_read(0x02);
        unsigned char hour = cmos_read(0x04);
        unsigned char day = cmos_read(0x07);
        unsigned char month = cmos_read(0x08);
        unsigned char year = cmos_read(0x09);
        unsigned char second_check = cmos_read(0x00);

        if (rtc_update_in_progress() || second != second_check) continue;

        int binary_mode = status_b & 0x04;
        int twenty_four_hour = status_b & 0x02;
        int pm = hour & 0x80;
        hour &= 0x7F;

        if (!binary_mode) {
            second = bcd_to_bin(second);
            minute = bcd_to_bin(minute);
            hour = bcd_to_bin(hour);
            day = bcd_to_bin(day);
            month = bcd_to_bin(month);
            year = bcd_to_bin(year);
        }

        if (!twenty_four_hour) {
            if (hour < 1 || hour > 12) return 0;
            hour %= 12;
            if (pm) hour += 12;
        }

        if (second > 59 || minute > 59 || hour > 23 ||
            day < 1 || day > 31 || month < 1 || month > 12 || year > 99) {
            return 0;
        }

        out->seconds = second;
        out->minutes = minute;
        out->hours = hour;
        out->day = day;
        out->month = month;
        out->year = year;
        return 1;
    }

    return 0;
}
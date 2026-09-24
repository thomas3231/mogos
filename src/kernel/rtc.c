#include "rtc.h"
#include "keyboard.h"   // reuses outb/inb

#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71
#define RTC_LOCAL_HOUR_OFFSET 2

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

void rtc_read(struct rtc_time* out) {
    // wait until the RTC isn't mid-update, to avoid reading torn/inconsistent values
    while (rtc_update_in_progress()) { }

    unsigned char second = cmos_read(0x00);
    unsigned char minute = cmos_read(0x02);
    unsigned char hour   = cmos_read(0x04);
    unsigned char day    = cmos_read(0x07);
    unsigned char month  = cmos_read(0x08);
    unsigned char year   = cmos_read(0x09);

    // most hardware/emulators default to BCD encoding -- convert to normal binary
    out->seconds = bcd_to_bin(second);
    out->minutes = bcd_to_bin(minute);
    out->hours   = (bcd_to_bin(hour) + RTC_LOCAL_HOUR_OFFSET) % 24;
    out->day     = bcd_to_bin(day);
    out->month   = bcd_to_bin(month);
    out->year    = bcd_to_bin(year);
}
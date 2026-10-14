#ifndef RTC_H
#define RTC_H

struct rtc_time {
    unsigned char seconds;
    unsigned char minutes;
    unsigned char hours;
    unsigned char day;
    unsigned char month;
    unsigned char year;   // last two digits only, e.g. 26 for 2026
};

int rtc_read(struct rtc_time* out);

#endif
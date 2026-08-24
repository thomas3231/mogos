# `rtc.c` / `rtc.h` — CMOS Real-Time Clock

A minimal driver for the CMOS RTC. Reads seconds, minutes,
hours, day, month, and year, converts from BCD to binary, and
fills in an `rtc_time` struct.

Used by `graphics.c::draw_status_text` to show the current
clock in the top-right of the status bar.

## Public API (`rtc.h`)

```c
struct rtc_time {
    unsigned char seconds;
    unsigned char minutes;
    unsigned char hours;
    unsigned char day;
    unsigned char month;
    unsigned char year;   // last two digits only, e.g. 26 for 2026
};

void rtc_read(struct rtc_time* out);
```

## Internals

```c
#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71
```

The two I/O ports are the standard CMOS index/data pair.
`outb(CMOS_ADDRESS, reg)` selects register `reg`; `inb(CMOS_DATA)`
returns the value.

```c
unsigned char cmos_read(unsigned char reg) {
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}
```

```c
int rtc_update_in_progress(void) {
    outb(CMOS_ADDRESS, 0x0A);
    return inb(CMOS_DATA) & 0x80;
}
```

Status register A (0x0A) bit 7 is the "update in progress" (UIP)
flag. When set, the RTC is mid-update and any read can return
torn values. The flag clears when the update finishes (within
~2 ms).

```c
unsigned char bcd_to_bin(unsigned char val) {
    return (val & 0x0F) + ((val / 16) * 10);
}
```

`val / 16` is the tens digit (BCD), `val & 0x0F` is the units
digit. Standard BCD-to-binary conversion. (Could be replaced
with `(val >> 4) * 10 + (val & 0x0F)`, but the compiler
generates the same code either way.)

## `rtc_read`

```c
void rtc_read(struct rtc_time* out) {
    while (rtc_update_in_progress()) { }

    unsigned char second = cmos_read(0x00);
    unsigned char minute = cmos_read(0x02);
    unsigned char hour   = cmos_read(0x04);
    unsigned char day    = cmos_read(0x07);
    unsigned char month  = cmos_read(0x08);
    unsigned char year   = cmos_read(0x09);

    out->seconds = bcd_to_bin(second);
    out->minutes = bcd_to_bin(minute);
    out->hours   = bcd_to_bin(hour);
    out->day     = bcd_to_bin(day);
    out->month   = bcd_to_bin(month);
    out->year    = bcd_to_bin(year);
}
```

Steps:

1. **Wait for the RTC to finish updating.** The `while` loop
   spins on the UIP flag. A timeout would be nice but isn't
   implemented; if the RTC hangs (it won't in QEMU) the kernel
   hangs too.
2. **Read all six fields.** The RTC's register layout uses
   `0x00, 0x02, 0x04, 0x07, 0x08, 0x09` for seconds, minutes,
   hours, day, month, year respectively. The 0x01/0x03/0x05
   alarm registers are skipped because the kernel doesn't
   program alarms.
3. **Convert each from BCD to binary.** Most emulators and
   real hardware default to BCD mode (status register B bit 2
   = 0). The conversion is unconditional — if the RTC is in
   binary mode the values are already binary and `bcd_to_bin`
   silently corrupts them. The comment in the file notes
   "most hardware/emulators default to BCD" — true, but
   worth being aware of.

## Caveats

- **No 12/24 hour handling.** Status register B bit 1 controls
  12-hour vs 24-hour mode. The driver always treats the hour
  byte as 24-hour binary; if the RTC is in 12-hour mode, PM
  hours (13..23) will be wrong.
- **No century register.** The year is two digits. The
  caller (`graphics.c`) prints it as a two-digit number; the
  century is hardcoded to 20.
- **No DST handling.** Whatever the RTC's timezone/DST
  setting is, that's what you get.
- **`outb`/`inb` come from `keyboard.h`.** Standard reuse
  pattern.
- **No interrupt-based update notification.** The driver
  polls the UIP flag. A more advanced implementation would
  enable the RTC's periodic interrupt (status register B bit
  6 + IRQ8) and use that to avoid the spin.

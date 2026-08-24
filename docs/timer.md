# `timer.c` / `timer.h` — PIT Driver

The PIT (Programmable Interval Timer) module drives the
uptime/cpu-usage counters in the status bar. It also supplies a
busy-wait helper (`timer_wait`) used in `kernel_main` to time
the boot beep.

## Public API (`timer.h`)

```c
void          timer_install(void);
unsigned int  timer_get_ticks(void);
unsigned int  timer_wait(unsigned int ticks);
void          timer_handler_c(void);

unsigned int  timer_get_uptime_seconds(void);
unsigned int  timer_get_cpu_usage_percent(void);
void          timer_set_idle_state(unsigned int is_idle);
```

## PIT programming

```c
void timer_install(void) {
    unsigned int divisor = 1193182 / 100;
    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
    set_idt_gate(32, (unsigned int)timer_handler_asm);
}
```

The PIT runs off a 1,193,182 Hz crystal. The divisor
`1193182 / 100 = 11931` gives an output frequency of 100 Hz
(period 10 ms per tick).

Port sequence:

- `0x43 = 0x36` — command byte: channel 0, low/high access
  mode, mode 3 (square wave), binary.
- `0x40 = divisor & 0xFF` — low byte of the divisor.
- `0x40 = (divisor >> 8) & 0xFF` — high byte of the divisor.

Then the handler is installed at IDT vector 32. Vector 32 is
the first vector after the CPU exception range, which is what
the remapped PIC uses for IRQ0.

`outb` and `set_idt_gate` come from `keyboard.h` (reused rather
than re-declared here).

## `timer_handler_c`

```c
volatile unsigned int tick_count       = 0;
volatile unsigned int uptime_seconds   = 0;
volatile unsigned int idle_ticks       = 0;
volatile unsigned int busy_ticks       = 0;
volatile unsigned int cpu_usage_percent = 0;
volatile unsigned int idle_state        = 0;

void timer_handler_c(void) {
    tick_count++;

    if (tick_count % 100 == 0) {
        uptime_seconds++;
    }

    if (idle_state) {
        idle_ticks++;
    } else {
        busy_ticks++;
    }

    if (tick_count % 100 == 0) {
        if (idle_ticks + busy_ticks != 0) {
            cpu_usage_percent = (busy_ticks * 100) / (idle_ticks + busy_ticks);
        }
        idle_ticks = 0;
        busy_ticks = 0;
    }
}
```

Runs on every PIT tick. The bookkeeping:

- `tick_count` increments every tick.
- Every 100 ticks (1 second), `uptime_seconds` increments.
- If `idle_state` is set, this tick counts as an idle tick;
  otherwise it counts as busy. `kernel_main` flips `idle_state`
  around the `hlt` instruction in the main loop.
- Every 100 ticks, the busy percentage is recomputed and the
  per-second counters are reset. `cpu_usage_percent` is the
  percentage over the last second.

All six globals are declared `volatile` because they're
written from the ISR context and read from the main loop. The
`volatile` qualifier prevents GCC from caching the value in a
register and missing the update.

The division-by-zero guard (`idle_ticks + busy_ticks != 0`)
shouldn't trigger in practice — every 100 ticks the ISR runs
100 times, so one of the two counters is always positive — but
the guard makes the code defensive.

## Read API

```c
unsigned int timer_get_ticks(void)            { return tick_count; }
unsigned int timer_get_uptime_seconds(void)   { return uptime_seconds; }
unsigned int timer_get_cpu_usage_percent(void){ return cpu_usage_percent; }
void timer_set_idle_state(unsigned int is_idle) { idle_state = is_idle; }
```

`graphics.c` calls `timer_get_uptime_seconds` and
`timer_get_cpu_usage_percent` from `draw_status_text` to format
the status bar. `kernel_main` calls `timer_set_idle_state(1)`
before `hlt` and `timer_set_idle_state(0)` after.

## `timer_wait`

```c
void timer_wait(unsigned int ticks) {
    unsigned int target = tick_count + ticks;
    while (tick_count < target) {
        __asm__ volatile ("hlt");
    }
}
```

A simple "wait N ticks" busy-wait. `kernel_main` uses
`timer_wait(15)` to time the boot beep (150 ms at 100 Hz).

The `hlt` is technically a no-op here — the loop would still
work without it — but it parks the CPU in a low-power state
until the next interrupt. The next PIT tick fires the
`timer_handler_c` ISR, which increments `tick_count`, and the
loop exits.

`hlt` is privileged and only legal in ring 0. The kernel runs
entirely in ring 0, so this is fine.

## Caveats

- **100 Hz hardcoded.** Changing the divisor or the rollover
  constant (`% 100`) requires updating both — the rollover is
  what makes the "100 ticks = 1 second" arithmetic work.
- **No PIT channel 1/2 use here.** Channel 2 is wired to the
  PC speaker in `speaker.c`; channel 1 is for DRAM refresh
  and the BIOS keeps ownership.
- **`cpu_usage_percent` is a coarse measurement.** It measures
  "ticks where `idle_state == 1`" vs "ticks where
  `idle_state == 0`". If `kernel_main` does work between
  `timer_set_idle_state(0)` and the next `hlt`, those ticks
  count as busy. If a future driver does work after
  `timer_set_idle_state(1)`, those ticks still count as idle —
  be careful with ordering if you add long-running handlers.
- **No counter overflow handling.** `tick_count` and
  `uptime_seconds` are 32-bit and will wrap after ~497 days
  (ticks) or ~136 years (seconds), respectively. Not a real
  problem.

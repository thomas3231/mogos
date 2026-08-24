# `kernel.c` — `kernel_main`

The entire `kernel.c` file is one function: `kernel_main`.
Everything else (drivers, the heap, the IDT) is initialized
elsewhere; this function is the boot orchestrator that wires
it all together.

```c
void kernel_main(void) {
    screen_init();
    heap_init();
    backbuffer_init();

    idt_install();
    exceptions_install();
    pic_remap();
    set_idt_gate(33, (unsigned int)keyboard_handler_asm);
    timer_install();

    fill_screen(0x10, 0x10, 0x10);
    render_status_frame();
    screen_swap();

    __asm__ volatile ("sti");

    beep(880);
    timer_wait(15);
    nosound();

    for (;;) {
        timer_set_idle_state(1);
        __asm__ volatile ("hlt");
        timer_set_idle_state(0);
        render_status_frame();
        screen_swap();
    }
}
```

## Step-by-step

### 1. `screen_init()`

Fills in the global `struct screen_info screen` from the VBE
assembly globals (`framebuffer_addr`, `screen_width`, etc.).
Must run before any other graphics function.

### 2. `heap_init()`

Sets up the heap bump pointer at `0x200000` and the end at
`0xA00000`. Must run before any `kmalloc`.

### 3. `backbuffer_init()`

Calls `kmalloc(pitch * height)` to allocate the off-screen
backbuffer. The size is `pitch * height` (e.g. 3 MiB at
1024×768×32 bpp). Must run after `heap_init` and after
`screen_init` (because `screen.pitch` and `screen.height`
need to be filled in).

### 4. `idt_install()`

Installs a fresh IDT (all zeros, then filled on demand by
`set_idt_gate`) and loads it via `lidt`. The base address is
adjusted by `+ 0x20000` so the IDTR points at the real
physical location of the table.

### 5. `exceptions_install()`

Registers the eleven CPU exception gates (`#DE`, `#DB`, `#NMI`,
`#BP`, `#OF`, `#BR`, `#UD`, `#NM`, `#DF`, `#GP`, `#PF`) using
the assembly stubs in `main.asm`.

### 6. `pic_remap()`

ICW1..ICW4 sequence that moves the master PIC's IRQs to
vectors 32..39 and the slave's to 40..47. Also unmasks
IRQ0 (timer), IRQ1 (keyboard), and IRQ12 (mouse, even though
no driver uses it).

### 7. `set_idt_gate(33, keyboard_handler_asm)`

Installs the keyboard ISR at vector 33. The handler is
declared `extern void keyboard_handler_asm(void);` at the top
of this file; the actual symbol is defined in `main.asm`.

### 8. `timer_install()`

Programs PIT channel 0 to 100 Hz and installs `timer_handler_asm`
at vector 32.

### 9. `fill_screen(0x10, 0x10, 0x10)`

Paints the entire backbuffer a dark gray. The color
`(0x10, 0x10, 0x10)` is the same gray used in `print_char`'s
backspace-erase, so any subsequent text will appear "on" the
background.

### 10. `render_status_frame()`

First call: draws the black status bar and the initial
status text. Subsequent calls throttle to 1 Hz and only
redraw when uptime or CPU% change.

### 11. `screen_swap()`

Copies the backbuffer into the VBE linear framebuffer. The
user sees the gray background with the status bar for the
first time.

### 12. `__asm__ volatile ("sti")`

Enables interrupts. From this point on, IRQ0 (timer) and
IRQ1 (keyboard) can fire.

### 13. `beep(880); timer_wait(15); nosound();`

Plays a 880 Hz tone (A5) for 150 ms (15 PIT ticks at
100 Hz). The `beep` sets up the PIT channel 2 + speaker gate,
the `timer_wait` busy-waits in a `hlt` loop, and `nosound`
clears the gate.

### 14. Main loop

```c
for (;;) {
    timer_set_idle_state(1);
    __asm__ volatile ("hlt");
    timer_set_idle_state(0);
    render_status_frame();
    screen_swap();
}
```

This is the kernel's idle loop. Each iteration:

1. Tells the timer driver "we are now idle" (so this tick
   counts as an idle tick for the CPU% calculation).
2. `hlt` — the CPU parks until the next IRQ fires. IRQ0
   (PIT tick, 100 Hz) and IRQ1 (keyboard press) are the
   normal sources of wakeup.
3. After waking, mark the CPU as "not idle" again. This
   resets the busy/idle accounting for the rest of the
   iteration.
4. `render_status_frame()` — throttled to ~1 Hz, repaints
   the status bar if uptime or CPU% changed.
5. `screen_swap()` — pushes the (possibly unchanged)
   backbuffer to the framebuffer.

The loop never exits. The kernel is the loop.

## Why this order matters

- `screen_init` → `heap_init` → `backbuffer_init`: the
  backbuffer depends on `screen.pitch` and `screen.height`,
  which are set by `screen_init`, and on the heap, which is
  set up by `heap_init`.
- `idt_install` before any `set_idt_gate`: the gates are
  written into the IDT array; `idt_load` then makes the array
  live.
- `exceptions_install`, `pic_remap`, `set_idt_gate(33, ...)`,
  `timer_install`: each one writes a gate or a mask. The order
  is otherwise flexible as long as they happen before `sti`.
- `fill_screen`/`render_status_frame`/`screen_swap` before
  `sti`: the first paint happens with interrupts disabled, so
  no ISR can run while the screen is half-drawn. Strictly
  speaking, only `screen_swap` is non-idempotent — but doing
  the whole initial draw before enabling IRQs is safer.
- `beep`/`timer_wait` after `sti`: `timer_wait` depends on
  IRQ0 firing, so interrupts must be on.
- The main loop is `hlt`-based: the CPU sleeps between
  interrupts. Without `sti` the `hlt` would never wake up
  (because the `sti` to set IF happened earlier).

## What's not in `kernel_main`

- **No scheduler.** There's only one execution context — the
  main loop. Adding multitasking would mean adding a timer
  preemption hook, a per-task stack, and a context switch.
- **No syscall interface.** There's no `int 0x80` or
  `sysenter` plumbing; user-mode code can't make system calls
  because there's no user mode.
- **No filesystem.** The kernel can't read the floppy
  anymore; the bootloader's `KERNEL.BIN` is the only file
  the kernel sees, and it's already in memory.
- **No userspace.** The kernel runs entirely in ring 0.
  There is no GDT entry for ring 3, no TSS, no `iret` to
  user mode.
- **No driver model.** Each driver is a free-standing pair
  of `*.c` / `*.h`. There's no `struct device`, no
  registration, no probe sequence. Adding a new device means
  writing two more files and wiring the initialization into
  `kernel_main`.

## Exit behaviour

The function has no `return`. The only ways it can leave the
main loop are:

- A CPU exception that the exception handler catches — the
  handler halts the CPU in a `cli; hlt` loop, so the kernel
  freezes.
- A triple fault (two CPU exceptions in a row) — the CPU
  resets. This happens if a fault occurs before the IDT is
  set up, or if an exception handler itself faults.
- An unrecoverable hardware error (very rare in QEMU).

In all three cases the kernel does not return control to
firmware. The only way to stop the simulation is to close
QEMU.

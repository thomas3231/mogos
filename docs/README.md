# MogOS Documentation

Detailed documentation for MogOS, a hobby 32-bit x86 operating
system. Every source file has a matching document; cross-cutting
documents cover the architecture, build system, and debug
workflow.

Start with `OVERVIEW.md`, then read `architecture.md` for the
memory map and boot flow.

## Contents

| File | Covers |
|---|---|
| `OVERVIEW.md` | What MogOS is, repo layout, boot flow at a glance, where to read next. |
| `architecture.md` | CPU mode timeline, physical memory map, linker layout, full boot flow. |
| `build-system.md` | Makefile, `run.sh`, full toolchain (including cross-compiler from source). |
| `interrupts.md` | IDT layout, PIC remap, IRQ vector map, hardware ISR flow. |
| `debugging.md` | QEMU + GDB recipes, common failure modes, debug print strategies. |
| `bootloader.md` | `src/bootloader/boot.asm` — the 512-byte FAT12 boot sector. |
| `kernel-entry.md` | `src/kernel/main.asm` — real→protected, VESA, GDT, IDT/IRQ stubs, exception stubs. |
| `linker-script.md` | `src/kernel/linker.ld` — section layout. |
| `kernel.md` | `src/kernel/kernel.c` — `kernel_main` boot orchestration. |
| `graphics.md` | `src/kernel/graphics.c/.h` — VESA framebuffer, bitmap font, status bar, backbuffer. |
| `heap.md` | `src/kernel/heap.c/.h` — free-list `kmalloc`/`kfree`. |
| `keyboard.md` | `src/kernel/keyboard.c/.h` — PS/2 keyboard, IDT/PIC helpers, `outb`/`inb`. |
| `timer.md` | `src/kernel/timer.c/.h` — PIT driver, uptime, CPU-usage accounting. |
| `rtc.md` | `src/kernel/rtc.c/.h` — CMOS RTC. |
| `speaker.md` | `src/kernel/speaker.c/.h` — PC speaker (PIT channel 2). |
| `palette.md` | `src/kernel/palette.c/.h` — 16-color VGA palette + grass green. |
| `exceptions.md` | `src/kernel/exceptions.c/.h` — CPU exception handler. |
| `host-tools.md` | `tools/fat/fat.c` — host-side FAT12 inspector. |

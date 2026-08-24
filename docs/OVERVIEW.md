# MogOS — Overview

MogOS is a hobby 32-bit x86 operating system. It boots from a 1.44 MB FAT12
floppy image, switches the CPU from real mode into 32-bit protected mode with a
flat memory model, sets up a VESA linear framebuffer, and runs a freestanding C
kernel with a minimal set of drivers (PS/2 keyboard, PIT timer, CMOS RTC, PC
speaker) and a CPU exception handler. A status bar drawn into the top of the
framebuffer shows uptime, a CPU-busy percentage, and the current RTC time.

The project was formerly called "NANOBYTE OS" / "NBOS" — the FAT12 volume label
(`ebr_volume_label`) and the floppy image filesystem label (`mkfs.fat -F 12 -n
"NBOS"`) still carry that name. The on-disk kernel file is `KERNEL.BIN` (FAT
short filename).

## Repository layout

```
MogOS/
├── Makefile                  # Build orchestration
├── run.sh                    # Launch in QEMU
├── README.md                 # Public-facing build & toolchain instructions
├── CLAUDE.md                 # Internal notes for AI coding agents (already exists)
├── test.txt                  # Example file copied onto the floppy image
├── src/
│   ├── bootloader/
│   │   └── boot.asm          # 512-byte real-mode FAT12 boot sector
│   ├── kernel/
│   │   ├── main.asm          # Entry stub: BIOS print, A20, VESA, PM switch,
│   │   │                     # GDT, IDT/PIC assembly trampolines, exception stubs
│   │   ├── kernel.c          # kernel_main() — boot orchestration
│   │   ├── linker.ld         # Section layout
│   │   ├── graphics.c/.h     # VESA framebuffer driver, bitmap font, status bar
│   │   ├── keyboard.c/.h     # PS/2 keyboard IRQ + IDT/PIC helpers
│   │   ├── timer.c/.h        # PIT (Programmable Interval Timer) IRQ + CPU usage
│   │   ├── rtc.c/.h          # CMOS real-time clock
│   │   ├── speaker.c/.h      # PC speaker (PIT channel 2)
│   │   ├── palette.c/.h      # 16-color VGA palette + grass green
│   │   ├── heap.c/.h         # kmalloc/kfree free-list allocator
│   │   └── exceptions.c/.h   # CPU exception handler (blue screen + halt)
│   └── tools/                # (none — tools live in tools/, not src/)
└── tools/
    └── fat/
        └── fat.c             # Host-side FAT12 inspector (compiled with host gcc)
```

Build output goes into `build/`:

```
build/
├── bootloader.bin            # 512-byte boot sector
├── kernel.bin                # Flat binary the bootloader loads
├── kernel_entry.o            # First ELF section: the real-mode entry stub
├── kernel.o                  # kernel_main
├── graphics.o / keyboard.o / heap.o / timer.o / palette.o /
│   exceptions.o / rtc.o / speaker.o
├── main_floppy.img           # 1.44 MB FAT12 image (boots in QEMU)
└── tools/
    └── fat                   # Host-side FAT12 inspector
```

## Boot flow at a glance

1. **BIOS → `boot.asm`.** BIOS loads the first 512 bytes of the floppy into
   physical `0x7C00` and jumps there in 16-bit real mode. The boot sector
   carries a FAT12 BIOS Parameter Block (BPB) so the BIOS/loader can read
   further sectors.

2. **`boot.asm` reads the root directory**, locates `KERNEL.BIN`, follows its
   FAT12 cluster chain, and copies the kernel into segment `0x2000:0x0000`
   (physical `0x20000`). On any read failure it halts and reboots on keypress.

3. **`main.asm` (still real mode).** Prints `Hello world from KERNEL!` via
   `int 0x10` teletype, enables the A20 gate via port `0x92`, queries VBE for
   mode info (`ax=0x4F01`, mode `0x118` = 1024×768×32 bpp linear framebuffer),
   sets the mode (`ax=0x4F02`, `bx=0x4118`), loads a tiny flat GDT, and sets
   `CR0.PE` to enter protected mode.

4. **`init_pm` (32-bit, protected mode).** Sets `ds/ss/es/fs/gs` to the data
   selector, points `ebp/esp` at `0x70000` (linear `0x90000` after the
   `0x20000` load offset), and calls `kernel_main`.

5. **`kernel_main` (C, protected mode).** Initializes the screen info struct,
   heap, backbuffer, IDT, exception stubs, and remaps the PIC. Installs the
   keyboard handler (vector 33) and the PIT timer (vector 32). Paints the
   background, draws the status bar, plays a 880 Hz beep, enables interrupts
   with `sti`, then enters the main loop: `hlt` in the idle state, redraw the
   status bar, `screen_swap`, repeat.

## Architectural properties

- **No libc, no hosted runtime.** All C is compiled with
  `-m32 -ffreestanding -fno-pie -fno-pic`. The kernel links with
  `i686-elf-ld` against a custom linker script and emits a flat binary
  (`--oformat binary`).
- **No memory management unit (MMU) setup.** No paging, no virtual memory —
  physical and linear addresses are the same. The linker script places
  everything at virtual `0x0`; the bootloader's load offset (`0x20000`) is
  added back by the assembly trampolines wherever the C code reads an
  assembly-defined symbol (see `gdt_descriptor`, `idt_install`,
  `backbuffer_init`).
- **No standard library headers.** The kernel includes its own
  `keyboard.h` for `outb`/`inb`, which every driver then reuses instead of
  declaring a generic port-IO header.
- **Single-CPU, no preemption.** There is no scheduler. The only running
  "task" is the main loop. The IRQ handlers run to completion before
  returning.

## Where to read next

- `architecture.md` — full memory map, CPU mode transitions, and the boot
  flow with addresses.
- `build-system.md` — how `make` and the toolchain fit together.
- `interrupts.md` — the IDT, the PIC remap, and the IRQ vector map.
- `bootloader.md`, `kernel-entry.md`, and the per-module files for
  module-by-module deep dives.
- `debugging.md` — running the OS under QEMU and attaching GDB.

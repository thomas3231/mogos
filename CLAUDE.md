# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project: MogOS

A hobby 32-bit x86 operating system (formerly "NANOBYTE OS" / "NBOS"). Boots from a FAT12 1.44 MB floppy image, switches the CPU from real mode to protected mode, sets up VESA framebuffer graphics, and runs a freestanding C kernel with basic drivers.

This repo currently has **no test suite, no linter, and no CI** — verification is running the OS in QEMU and watching what happens.

## Toolchain (Ubuntu/WSL)

Required tooling, all from `apt`:

- `nasm` — bootloader and kernel entry stub
- `i686-elf-gcc` (13.2.0) + `i686-elf-ld` — cross-compiler for freestanding C; **must be built from source** as `~/opt/cross/bin`. System `gcc` will not work for the kernel (it doesn't target bare-metal i386). See README for the binutils + GCC cross-compile recipe.
- `mtools` — copies `kernel.bin` onto the FAT12 image (`mcopy`)
- `dosfstools` — provides `mkfs.fat` for FAT12 formatting
- `qemu-system-x86` — emulator; `qemu-system-i386` runs the floppy image
- `gdb` — optional kernel debugging

`PATH` must include `~/opt/cross/bin` so `i686-elf-gcc` and `i686-elf-ld` resolve.

## Common commands

All commands run from the repo root (`~/MogOS` in WSL).

| Task | Command |
|---|---|
| Build everything (floppy image + host tool) | `make` |
| Run in QEMU | `./run.sh` (equivalent to `qemu-system-i386 -fda build/main_floppy.img`) |
| Build only the kernel ELF/bin | `make kernel` |
| Build only the bootloader | `make bootloader` |
| Build only the FAT host tool | `make tools_fat` |
| Clean all build artifacts | `make clean` |

There is **no `make test`** target — there's no automated test infrastructure.

### Building a single object file

If you only need to rebuild one module after editing (faster than `make kernel` when only one `.c` changed, since the Makefile rebuilds everything via `always:`):

```
make build/<name>.o     # e.g. make build/heap.o
```

After that, link with `make build/kernel.bin`.

### Debugging in QEMU + GDB

Not in the Makefile — start QEMU manually:

```
qemu-system-i386 -fda build/main_floppy.img -S -gdb tcp::1234
# in another shell:
gdb -ex "target remote :1234" -ex "set architecture i386"
```

## Architecture

### Boot flow

1. **`src/bootloader/boot.asm`** (real-mode, FAT12). BIOS loads the 512-byte bootloader at `0x7C00`. It parses the BPB, reads the root directory, finds `KERNEL.BIN`, follows the FAT chain cluster-by-cluster, loads the kernel into segment `0x2000:0x0000`, then far-jumps to it. On error it halts and waits for a keypress before rebooting.
2. **`src/kernel/main.asm`** (real-mode → protected-mode). The kernel entry stub prints a message via BIOS `int 0x10`, enables the A20 gate (port 0x92 fast path), queries VBE for mode `0x118` (1024×768×32bpp linear framebuffer) via `int 0x10`/`ax=0x4F01`, sets that mode via `ax=0x4F02`, then loads a tiny flat GDT and switches to protected mode by setting CR0.PE and far-jumping through `gdt_code`. The framebuffer address (from VBE) is stashed in the global `framebuffer_addr` so C can read it.
3. **`src/kernel/kernel.c::kernel_main`** (protected-mode C). Sets up the screen info struct from the globals `main.asm` filled in, initializes the heap, installs the IDT, remaps the PIC, wires the keyboard IRQ (vector 33), installs the PIT timer, fills the screen, draws a status bar, enables interrupts with `sti`, then loops on `hlt`/redraw.

### Memory layout (linker)

`src/kernel/linker.ld` loads everything at virtual address `0x0`. The bootloader puts the kernel image at physical `0x20000` (segment `0x2000` × 16). The entry stub `main.asm` is linked first so it sits at the start of the `.text` section. The 32-bit stack is set up at `0x70000` (note the comment in `main.asm`: the calculation accounts for the `0x20000` load offset, so the linear address is `0x90000`).

### Kernel module map (`src/kernel/`)

| File | Role |
|---|---|
| `main.asm` | Entry stub: BIOS print, A20, VESA mode set, PM switch, IDT/IRQ assembly trampolines, exception stubs. Defines `kernel_main` caller and `idt_load`. |
| `kernel.c` | `kernel_main` — boot orchestration only; no logic lives here. |
| `linker.ld` | Section layout (entry stub first, then `.text`/`.data`/`.rodata`/`.bss`). |
| `graphics.c/.h` | VESA framebuffer driver. Reads `framebuffer_addr`/`screen_width`/etc. exported by `main.asm` into the `screen` global, then provides `put_pixel`, `draw_rect`, `fill_screen`, `draw_char` (scaled bitmap font), and a status-bar / status-text API used by the main loop. |
| `palette.c/.h` | VGA-style 16-color palette (likely used for text rendering in 16-color modes — verify before removing). |
| `keyboard.c/.h` | PS/2 keyboard IRQ handler + `set_idt_gate`. Decodes scancode set 1. |
| `timer.c/.h` | PIT (programmable interval timer) IRQ handler; idle-state tracking for the main `hlt` loop. |
| `rtc.c/.h` | CMOS real-time clock reads (used by the status bar). |
| `heap.c/.h` | `kmalloc`/`kfree` — simple free-list heap. |
| `exceptions.c/.h` | CPU exception handler. Paints a blue screen with the exception name; halts. Installs stubs for vectors 0–8, 13, 14 (matches the `EXCEPTION_NOERR`/`EXCEPTION_ERR` macros in `main.asm`). |

### Interrupts

- The IDT is built and loaded from C (`idt_install` in keyboard/timer modules). Use `set_idt_gate(n, addr)` to register.
- The PIC is remapped so IRQs start at vector 32. Vector 33 = keyboard (IRQ1), vector 32 = timer (IRQ0).
- All hardware ISR assembly stubs (`keyboard_handler_asm`, `timer_handler_asm`) push `a..d`/di/si/bp, call a C handler, then send EOI (`0x20` to PIC command port) and `iretd`.
- CPU exception stubs (`exception_stub_*`) are generated by `EXCEPTION_NOERR` / `EXCEPTION_ERR` macros in `main.asm` and call `exception_handler_c(vector)`.

### Host tool

- **`tools/fat/fat.c`** — a userspace C program compiled with the host `gcc` (not the cross-compiler) for inspecting the FAT12 image. Built into `build/tools/fat` by the `tools_fat` target.

### Conventions / gotchas for future edits

- All kernel C is compiled with `-m32 -ffreestanding -fno-pie -fno-pic`. No libc, no hosted runtime, no standard headers beyond what the cross-gcc ships with freestanding.
- When adding a new `.c` file to the kernel, you must add an explicit rule in the Makefile **and** append the new `.o` to the `kernel.bin` link line — there is no pattern rule, and the Makefile won't auto-discover sources.
- Every kernel build rule depends on `always`, which only creates the `build/` directory — they do not track header dependencies. After changing a `.h`, rebuild with `make clean && make kernel`.
- The kernel and bootloader are linked with `--oformat binary`; symbols and relocations are gone after linking. Debugging kernel C requires either QEMU + GDB against an ELF or careful disassembly.
- Framebuffer is 32 bpp at the VBE-reported `framebuffer_addr` — `graphics.c` writes BGRA bytes directly. Don't assume the VBE byte order is RGB; check `screen.bpp` / pixel writes before changing.
- Status-bar text updates happen from `render_status_frame` — adding new runtime info (CPU usage, memory, etc.) means wiring a producer that the main loop calls before `render_status_frame`.

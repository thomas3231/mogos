# Debugging — QEMU + GDB

MogOS has no kernel debugger, no test suite, and no logging
framework. The two debug strategies are: (1) look at the
QEMU window, and (2) attach GDB to QEMU and step through the
kernel. The Makefile does not have a `make debug` target, so
this is all by hand.

## Build artifacts to know about

| File | Format | Notes |
|---|---|---|
| `build/bootloader.bin` | flat 512-byte binary | what `dd` writes to sector 0 of the floppy |
| `build/kernel.bin` | flat binary | what the bootloader loads; no symbols, no relocations |
| `build/*.o` | ELF32 | has symbols, but only useful as long as the matching `.bin` was built from the same object files |
| `build/main_floppy.img` | 1.44 MB FAT12 image | the bootable thing |

Because the kernel is a **flat binary** with no symbols, GDB
cannot debug `kernel.bin` directly. The practical workflow is
to either:

1. Debug the bootloader with the BIOS, or
2. Add a debug build of the kernel that emits an ELF.

The Makefile doesn't currently have a debug-ELF target. The
recipe below is for the QEMU+GDB manual setup.

## Running QEMU under GDB control

In one shell, start QEMU paused:

```bash
qemu-system-i386 -fda build/main_floppy.img -S -gdb tcp::1234
```

- `-S` — start the CPU paused. The OS will not run until GDB
  resumes it (or you can omit `-S` and use GDB to break in
  later).
- `-gdb tcp::1234` — open a GDB stub on TCP port 1234.

The audio flag from `run.sh` is optional during debug; you can
add it if you want to hear the boot beep while stepping:

```bash
qemu-system-i386 -fda build/main_floppy.img -S -gdb tcp::1234 \
                 -audiodev pa,id=snd0 -machine pcspk-audiodev=snd0
```

In a second shell, attach GDB:

```bash
gdb -ex "target remote :1234" -ex "set architecture i386"
```

The `set architecture i386` is important — by default GDB
infers the architecture from the ELF on disk, and if you
attach without a debug ELF it may guess wrong. The first
GDB prompt will appear in the bootloader; type `c` (continue)
or `si` to start.

### Useful initial breakpoints

The bootloader is hard to debug from a protected-mode GDB
because the CPU starts in real mode and the symbols for
`start:` are inside the flat binary. The pragmatic options:

- **Break at the kernel entry.** Add a temporary
  `int 3` (opcode `0xCC`) at the top of `start:` in
  `main.asm`:
  ```asm
  start:
      int 3
      mov si, msg_hello
      ...
  ```
  Rebuild, run QEMU under GDB, and GDB will break at the
  `int 3` with `eip` inside `start:`. Remove the `int 3`
  before committing.

- **Use the BIOS.** QEMU's built-in GDB stub is one stub
  for the whole VM, not per-CPU-mode. You can break on BIOS
  entry points; for example, `hb *0x7C00` breaks when the
  BIOS jumps to the boot sector. (Requires GDB to know the
  symbol; for a 16-bit real-mode target, the stub reports
  the current CS:IP correctly.)

- **Step the bootloader.** QEMU's GDB stub doesn't single-step
  the 16-bit boot sector very well; it's easier to put a
  known break in `boot.asm` and step from there.

### Protected-mode debugging

Once `switch_to_pm` runs, the GDB stub sees the 32-bit
protected-mode state. The selector registers (`cs`, `ds`, ...)
all read as the new GDT selectors (`0x08`, `0x10`). EIP
points into the kernel image at offset `0 + (eip - 0x20000)`
in terms of the flat binary's virtual address.

If you've built the kernel with debug symbols into ELF object
files (the default for `i686-elf-gcc -g`), you can `add-symbol-file
build/kernel_entry.o 0x20000` and then `add-symbol-file
build/graphics.o 0x20000+sizeof(kernel_entry.o)`, etc. —
matching the order from the Makefile's link line.

## Useful GDB commands for MogOS

| Command | What it does |
|---|---|
| `info registers` | Dump the 8 GP regs + segment regs + EIP/EFLAGS |
| `info gdt` | (qemu-specific) Print the GDT |
| `x/16i $eip` | Disassemble 16 instructions at the current EIP |
| `x/64xb 0x20000` | Hexdump 64 bytes at the kernel load address |
| `break *0x20000` | Set a breakpoint at the kernel's first instruction |
| `watch tick_count` | Stop on any write to `tick_count` |
| `print/x $esp` | Print the stack pointer |
| `info idt` | (qemu-specific) Print the IDT |

## Debugging the bootloader

The bootloader is too small for a debugger to be useful; a
serial port print or a single `int 19h` (reboot) is usually
enough. Common failures:

| Symptom | Likely cause |
|---|---|
| QEMU says `Boot failed: could not read boot disk` | `bootloader.bin` is not at sector 0 — check the `dd conv=notrunc` step. |
| QEMU says `KERNEL.BIN file not found!` | The `mcopy` step didn't run, or the file is not in the root directory. Run `build/tools/fat build/main_floppy.img KERNEL  BIN` to inspect. |
| QEMU hangs after `Loading...` | A read error in the FAT chain walk. Check the FAT table with `build/tools/fat` against an expected image. |
| QEMU doesn't show `Hello world from KERNEL!` | The kernel landed in memory but the real-mode print isn't reaching the BIOS. Check that the `puts` function and `int 0x10` are wired correctly. |

## Debugging the kernel after `sti`

After `sti`, the main loop runs and the keyboard/timer ISRs
fire on every relevant event. Common failures:

| Symptom | Likely cause |
|---|---|
| Nothing on the screen, but the boot beep played | `screen_swap` is broken or the framebuffer address is wrong. The boot beep is driven by PIT channel 2 + speaker, not the framebuffer. Check `framebuffer_addr` from VBE. |
| Status bar missing or garbled | A draw happened before `screen_init`. |
| CPU% reads `0` permanently | `timer_set_idle_state(1)` is never called, or the loop never reaches `hlt`. |
| Keyboard types but the text doesn't appear | `print_char` is broken, or the keyboard handler is at the wrong vector. |
| Kernel reboots spontaneously after a few seconds | A double fault (an exception that fires inside the exception handler). Often a bad pointer in a draw routine. |

## Adding debug prints to the kernel

There's no `printf` and no serial port driver. The easiest
way to get text out is one of:

1. **Write to the backbuffer via `print_char` and `screen_swap`.**
   Fine for tracing at low rates, but synchronous and slow.
2. **Add a serial port driver.** A simple `outb(0x3F8, c)` is
   enough to talk to QEMU's emulated 16550. The other end
   of the serial port can be QEMU's `-serial stdio` or a
   pty. This is the standard "OS dev printf" pattern.
3. **Use QEMU's debug port.** `outb(0x402, c)` writes a byte
   to QEMU's debugcon (configure with `-debugcon stdio`).
   Cheaper than a full serial driver.

For deep tracing, option 2 is the most flexible.

## Resetting the VM

If the kernel hangs in a state where GDB can't break (e.g.
the GDB stub isn't connected yet, or the kernel is in a
`cli; hlt` loop), kill and restart QEMU. There's no
soft-reset path through the kernel itself — the only reset
mechanism is the warm reboot at the end of the bootloader's
`wait_key_and_reboot`, which the kernel never reaches.

## Memory inspection tips

- `x/16xg 0x20000` — first 16 64-bit words of the kernel
  image. Should start with the bytes of `start:` in
  `main.asm` (`mov si, msg_hello` = `BE 7C 20` etc.).
- `x/64xb $esp-32` — 64 bytes of stack starting just
  below the current stack pointer.
- `print/x *(int*)0x20000` — read the first 4 bytes as a
  little-endian 32-bit integer.

## When in doubt, add `int 3`

A single `int 3` (opcode `0xCC`) anywhere in the kernel is
the cheapest possible breakpoint. The CPU halts, the GDB
stub takes over, and you can `info registers`, single-step,
or `c` from there. `int 3` doesn't need IDT entry 3 to be
installed (it raises a #BP, but QEMU's GDB stub intercepts
the #BP before the IDT gets a chance to fire). If you want
GDB to actually break, you need:

- `break *<address of int 3>` from the GDB side, **or**
- A real `int 3` handler that the kernel's exception stub
  calls into, which then signals GDB via `outb` to the
  debugcon.

The first is simpler.

# `src/kernel/linker.ld` — Linker Script

A 9-line linker script that controls how the kernel object
files are placed in the flat output binary.

```ld
ENTRY(start)
SECTIONS
{
    . = 0x0;
    .text : {
        build/kernel_entry.o(.text)
        *(.text)
    }
    .data : { *(.data) }
    .rodata : { *(.rodata) }
    .bss : { *(.bss) }
}
```

## `ENTRY(start)`

The linker records `start` as the program entry point. The
bootloader doesn't actually use the ELF entry point — it
jumps to `0x2000:0x0000` blindly — but the entry is used by
any debug tool that consumes the ELF, and it's required for
the link to succeed.

`start` is the label at the top of `main.asm`:

```asm
global start
start:
    mov si, msg_hello
    call puts
    ...
```

## `SECTIONS` block

### `. = 0x0;`

Sets the location counter (the "current address") to zero.
Every symbol defined from this point on will have a virtual
address starting at `0x0`. This is the key fact: **all C
symbols in the kernel are zero-based.** Anything that needs
the real physical address must add `0x20000` manually.

### `.text` — first

```ld
.text : {
    build/kernel_entry.o(.text)
    *(.text)
}
```

The `.text` section is constructed by **first** pulling in
every input `.text` section from `kernel_entry.o`, then every
other input `.text` from the rest of the object files. This
ensures that `main.asm`'s real-mode entry stub sits at the
very start of the binary — the byte at offset 0 of the flat
binary is the first byte of `start:`.

The Makefile assembles `main.asm` into `build/kernel_entry.o`
and links it first in the link line, so its `.text` ends up
first inside the section. The C `.o` files (kernel, graphics,
keyboard, ...) follow.

### `.data`, `.rodata`, `.bss` — standard

```ld
.data   : { *(.data) }
.rodata : { *(.rodata) }
.bss    : { *(.bss) }
```

Each section pulls in the matching input section from every
object file. The data section holds `palette[]` and the
`font8x8` table (in `graphics.c`). The rodata section holds
read-only data (string literals, the `exception_names`
array). The bss section is zero-initialized storage for
uninitialized globals (the `idt[]` array, the heap
bookkeeping, the screen info struct, ...).

No explicit alignment is given — the linker uses 4-byte
alignment by default for i386. No symbols are defined for
the start/end of each section because the kernel doesn't
use them; nothing in the source code refers to `__bss_start`
or similar.

## Why the script is this small

A full OS linker script usually has separate PHDRs for
`text`, `data`, `LOAD`, etc., an explicit stack region, a
heap region, a separate section for uninitialized data
that's pre-zeroed at boot, and possibly a separate section
for read-only data. MogOS doesn't need any of that:

- The flat binary has no program headers (`--oformat binary`).
- The stack is set up by `init_pm` directly (`ebp = esp =
  0x70000`).
- The heap is allocated at a hardcoded address by
  `heap_init`.
- The BSS is zero-initialized by the C runtime (or rather,
  by the fact that `kmalloc`-style memory is already zero
  in the `kmalloc` carve region — `kernel_entry.o` doesn't
  zero the BSS, but nothing in the kernel relies on BSS
  being zero before `heap_init` runs, because every BSS
  global is assigned before it's read).

The linker script's only jobs are:

1. Place `kernel_entry.o` first in `.text` (so the
   bootloader jumps to the right place).
2. Set the virtual address base to `0x0` (so C symbols are
   zero-based and `+ 0x20000` adjustments work uniformly).

Both of these are handled by the script above. Anything
more would be over-engineered for the current source size.

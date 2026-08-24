# Architecture & Memory Map

This document describes what the CPU is doing at each stage of the boot and
where every important thing lives in physical memory.

## CPU mode timeline

| Stage | Code | CPU mode | Segment layout | Where control lives |
|---|---|---|---|---|
| 1. BIOS post → boot | `boot.asm` (512 B) | 16-bit real | `ds=es=ss=0` | `cs:ip = 0x0000:0x7C00` |
| 2. Bootloader runs | `boot.asm` | 16-bit real | `ds=es=ss=0` | `cs:ip = 0x0000:0x7C00` |
| 3. Bootloader hands off | jump to `KERNEL_LOAD_SEGMENT:0` | 16-bit real | `ds=es=0x2000` | `cs:ip = 0x2000:0x0000` |
| 4. Kernel entry | `main.asm::start` | 16-bit real | same as 3 | `0x20000` |
| 5. VESA / A20 / GDT | `main.asm` | 16-bit real | same | `0x20000` |
| 6. Switch to PM | `switch_to_pm` → `init_pm` | 32-bit protected, flat | `ds/ss/es/fs/gs = 0x10` | `0x20000` (linear) |
| 7. `kernel_main` | `kernel.c` | 32-bit protected, flat | same | `0x20000` (linear) |
| 8. Main loop | `kernel.c` | 32-bit protected, flat, IRQs on | same | `0x20000` (linear) |

Mode switch happens in `switch_to_pm` (real) → `init_pm` (protected). The
transition is the standard "set `CR0.PE`, then `jmp CODE_SEG:init_pm`" trick —
the far jump flushes the prefetch queue and forces the CPU to start decoding
32-bit instructions.

## Physical memory map

The kernel does not set up paging, so **linear address = physical address** at
all times. The numbers below are physical.

| Range | Owner | Notes |
|---|---|---|
| `0x00000 – 0x003FF` | Real-mode IVT | Untouched; we overwrite parts with the IDT later. |
| `0x00400 – 0x004FF` | BIOS data area | Left alone. |
| `0x00500 – 0x07BFF` | Conventional (free at boot) | Used by the bootloader as `buffer` for FAT/dir sectors (see below). |
| `0x07C00 – 0x07DFF` | **Boot sector** | `boot.asm` runs from here. `org 0x7C00`. |
| `0x07E00 – 0x1FFFF` | Free at boot | The bootloader writes its FAT12 staging buffer at `0x7E00` (right after the boot sector), so disk sectors it reads land here. |
| `0x20000 – ???` | **Kernel image** | Loaded by the bootloader into `0x2000:0x0000` (segment:offset). Includes `kernel_entry.o`, all `.text`, `.data`, `.rodata`, `.bss`. With the current source size (≈ 10 modules, ~2 KLOC), the image is well under 256 KB. |
| `0x70000` (logical) → `0x90000` (linear) | **Kernel stack** | `ebp/esp` set in `init_pm` to `0x70000`. The offset is because the kernel was loaded at `0x20000`; the comment in `main.asm` makes this explicit. Stack grows down. |
| `0xA0000 – 0xFFFFF` | VGA/ROM | Not used (VBE framebuffer lives elsewhere; see below). |
| VBE-reported address | **Linear framebuffer** | Stored in `framebuffer_addr` (`main.asm`) and `screen.framebuffer` (`graphics.c`). For mode `0x118` (1024×768×32bpp) QEMU/Bochs typically reports something like `0xFD000000` or a host-VRAM address. |
| `0x200000 – 0xA00000` | **Kernel heap** | `HEAP_START = 0x200000`, `HEAP_SIZE = 0x800000` (8 MB) in `heap.c`. The `kmalloc` free list carves blocks out of this region. **Assumes no other code uses this 8 MB window** — true on QEMU's default i440FX machine with no highmem, but be aware before changing. |

### Where each module's globals live

- `screen`, `backbuffer`, `term_cursor_x/y`, `text_scale` — `.bss`/`.data` of
  `graphics.o`, linked into the kernel image.
- `idt[256]`, `idtp` — `.bss` of `keyboard.o`, linked into the kernel image.
  `idt_load` is given `&idtp + 0x20000` so the IDTR points at the actual
  physical location of the table.
- `tick_count`, `uptime_seconds`, `cpu_usage_percent`, `idle_ticks/busy_ticks`,
  `idle_state` — `.bss` of `timer.o`.
- `heap_start_block`, `heap_uncarved`, `heap_end` — `.bss` of `heap.o`. They
  all default to the absolute addresses `0x200000` / `0xA00000`.
- `ctrl_pressed`, `alt_pressed`, `shift_pressed` — `.bss` of `keyboard.o`.

## Linker layout (`src/kernel/linker.ld`)

```
ENTRY(start)
SECTIONS
{
    . = 0x0;
    .text : {
        build/kernel_entry.o(.text)   /* main.asm's real-mode entry FIRST */
        *(.text)                     /* everything else (kernel.c, drivers) */
    }
    .data : { *(.data) }              /* palette table, font8x8 (graphics.c) */
    .rodata : { *(.rodata) }
    .bss : { *(.bss) }
}
```

The crucial detail: `kernel_entry.o` is listed first inside `.text` so that
`main.asm`'s real-mode entry (`start:`) lands at the very first byte of the
flat binary. The bootloader copies the entire binary into memory starting at
`0x20000`, so when the bootloader does `jmp 0x2000:0x0000` execution begins
exactly at the start of `start:`. The linker virtual address is `0x0` for
every section, so C code that takes the address of a function gets the
in-binary offset; wherever the actual physical address is needed the
`0x20000` load offset is added manually (`gdt_descriptor` base, `idt_load`
pointer, `backbuffer_init`).

Note also that the C code's symbol references resolve to those same
zero-based offsets. There is no relocation processing — the kernel is a flat
binary, not an ELF. If you ever switch to a non-zero link base, every
`+ 0x20000` in the assembly/C has to move with it.

## Boot flow in detail

### 1. Real-mode bootloader (`src/bootloader/boot.asm`)

- BIOS POST loads the boot sector at `0x7C00` and jumps to it.
- The boot sector itself begins with a FAT12 BPB (BIOS Parameter Block) so
  the sector is also parseable as a FAT header. The BPB records
  `BytesPerSector=512`, `SectorsPerCluster=1`, `ReservedSectors=1`,
  `FatCount=2`, `DirEntriesCount=0xE0` (224), `TotalSectors=2880`,
  `MediaDescriptorType=0xF0` (3.5" floppy), `SectorsPerFat=9`,
  `SectorsPerTrack=18`, `Heads=2`, volume label `NANOBYTE OS`,
  system ID `FAT12   `.
- After saving `dl` (the BIOS drive number) and printing `Loading...`, the
  bootloader uses `int 0x13 ah=08h` to query the actual drive geometry and
  overwrites `SectorsPerTrack` / `Heads` with what the BIOS reports.
- It computes the LBA of the root directory
  (`reserved + fat_count * sectors_per_fat = 1 + 18 = 19`), reads it into
  `buffer` (right after the boot sector in memory), and scans the 224
  32-byte directory entries for one whose first 11 bytes equal
  `KERNEL  BIN`.
- Once found, it stores the file's first cluster in `kernel_cluster`, then
  reads the FAT itself into `buffer` (the FAT lives in sector 1 — the same
  buffer is reused; this works because the bootloader has already finished
  scanning the root directory).
- It loops over the FAT chain. For each cluster it computes the absolute
  sector number with the hardcoded offset 33 (1 reserved + 18 fat + 14
  root-directory sectors = 33; the comment in the file notes this), reads
  one sector into `es:bx` (starting at `0x2000:0x0000`), and advances `bx`
  by `BytesPerSector`. After each read it indexes the FAT to find the next
  cluster: `index = cluster * 3 / 2`, then if `cluster` is even it takes
  the low 12 bits, otherwise the high 12 bits — the standard FAT12
  pack-into-1.5-bytes trick. The loop stops at `>= 0x0FF8`.
- Finally it sets `ds=es=KERNEL_LOAD_SEGMENT (0x2000)` and
  `jmp 0x2000:0x0000`. Any read error jumps to `floppy_error` which prints
  a message, waits for a keypress, and reboots via `jmp 0xFFFF:0`.

### 2. Real-mode kernel entry (`src/kernel/main.asm::start`)

- Prints `Hello world from KERNEL!` via `int 0x10 ah=0x0E`.
- `enable_a20` writes port `0x92` to enable the A20 gate. The fast-path
  check (`test al, 2; jnz .after`) avoids disabling the A20 gate if it's
  already on. If bit 1 needs to be set it ORs it in; it also clears bit 0
  so we don't accidentally reset the CPU.
- `set_vesa_mode` calls VBE:
  - `ax=0x4F01, cx=0x118, es:di=vbe_mode_info` — get mode info. The 256-byte
    `vbe_mode_info` struct is filled in. Offsets used:
    - `+16` → `pitch` (bytes per scan line)
    - `+18` → `width` (pixels)
    - `+20` → `height` (pixels)
    - `+25` → `bpp` (bits per pixel)
    - `+40` → `framebuffer` physical address (32-bit)
  - `ax=0x4F02, bx=0x4118` — set mode `0x118` with the linear-framebuffer
    bit (`0x4000`) set. If the BIOS returns anything other than `0x004F`
    in `ax`, the routine jumps to `msg_vesa_failed` and halts.
  - The four values are stored in assembly globals (`framebuffer_addr`,
    `screen_width`, `screen_height`, `screen_pitch`, `screen_bpp`) so C
    can `extern` them.
- `switch_to_pm` builds a tiny GDT right in `.text`:
  - Null descriptor (`gdt_start`).
  - `gdt_code`: base=0, limit=0xFFFFF, granularity=4 KiB, 32-bit, ring 0,
    code, read/write. Selector offset = `CODE_SEG = 0x08`.
  - `gdt_data`: same base/limit/granularity, 32-bit, ring 0, data, expand-up.
    Selector offset = `DATA_SEG = 0x10`.
  - `gdt_descriptor` carries `limit` and `base` — the `base` already
    includes `+ 0x20000` to point at the real physical location.
  - `lgdt [gdt_descriptor]; mov eax,cr0; or eax,1; mov cr0,eax; jmp
    CODE_SEG:init_pm`.

### 3. Protected-mode setup (`init_pm`)

- Reloads all data segment registers with `DATA_SEG` so the flat data
  selector takes effect.
- Sets `ebp = esp = 0x70000`. The comment in the file explains the math:
  the kernel was loaded at `0x20000`, so the C-visible "linear" address
  of the stack pointer is `0x20000 + 0x70000 = 0x90000`. This works
  because the flat model means the linear address is what physical
  memory actually uses.
- Calls `kernel_main` (in C).

### 4. `kernel_main` (`src/kernel/kernel.c`)

- `screen_init()` — copy `framebuffer_addr`/`screen_width`/`screen_height`/
  `screen_pitch`/`screen_bpp` from the assembly globals into the global
  `struct screen_info screen`. **Note:** `screen.framebuffer` is set to
  `framebuffer_addr - 0x20000` because every other layer adds `0x20000`
  to recover the actual physical address — see `backbuffer_init` and
  `screen_swap` for the inverse.
- `heap_init()` — set `heap_uncarved = 0x200000`, `heap_end = 0xA00000`.
- `backbuffer_init()` — allocate the backbuffer
  (`kmalloc(pitch * height)`).
- `idt_install()` — fill the 256-entry IDT with zeros, set `idtp.limit` to
  `sizeof(idt_entry)*256 - 1`, set `idtp.base = (unsigned int)&idt +
  0x20000`, and call `idt_load(&idtp)` (assembly). The `+ 0x20000`
  compensates for the bootloader's load address.
- `exceptions_install()` — register stubs for CPU exception vectors
  `0..8, 13, 14` (matches the `EXCEPTION_NOERR` / `EXCEPTION_ERR` macros
  in `main.asm`).
- `pic_remap()` — ICW1..ICW4 sequence that moves IRQ0..7 to vectors
  32..39 and IRQ8..15 to vectors 40..47, and unmasks IRQ0 (timer),
  IRQ1 (keyboard), and IRQ12 (mouse, even though no driver uses it).
- `set_idt_gate(33, keyboard_handler_asm)` — install the keyboard ISR.
- `timer_install()` — program PIT channel 0 for ~100 Hz (divisor
  `1193182/100`) and install its handler at vector 32.
- Paint background, draw the status bar once, swap the backbuffer, then
  `sti` (enable interrupts).
- Beep 880 Hz for ~150 ms (`timer_wait(15)` = 15 ticks at 100 Hz).
- Main loop: mark the CPU idle, `hlt` (sleep until next IRQ), mark the
  CPU busy, `render_status_frame`, `screen_swap`. The CPU usage shown
  in the status bar is computed from idle vs. busy ticks over the last
  second.

## Mode/IRQ contracts that aren't obvious

- **No paging** means `kmalloc`'s `0x200000` base is a physical address.
  If you ever add paging, every reference to `HEAP_START` / `HEAP_END` /
  the framebuffer address / the stack pointer / `gdt_descriptor.base`
  / `idt_install` needs to become a virtual address instead.
- **The IDT/PIC live in physical memory at the kernel's load address.**
  The IDT's physical base is `(unsigned int)&idt + 0x20000`. If the
  bootloader load address ever changes, this `+ 0x20000` has to follow.
- **IRQs run with the same flat selectors as the rest of the kernel.**
  No separate IRQ task gate or IST — each hardware ISR uses the same
  `pusha; call C; send EOI; popa; iretd` pattern from `main.asm`.

# Kernel Entry (`src/kernel/main.asm`)

`main.asm` is the kernel's real-mode → protected-mode trampoline plus
the assembly-side interrupt machinery. It is linked first into the
flat binary, so when the bootloader does
`jmp 0x2000:0x0000` execution begins at `start:` in this file.

The file is a mix of 16-bit real-mode and 32-bit protected-mode code,
switched back and forth with NASM's `[bits 16]` / `[bits 32]`
directives.

## Section map

| Phase | Code | `[bits]` |
|---|---|---|
| Entry, BIOS print | `start`, `puts` | 16 |
| A20 enable | `enable_a20` | 16 |
| VESA mode set | `set_vesa_mode` | 16 |
| GDT (inline data) | `gdt_start`, `gdt_code`, `gdt_data`, `gdt_end`, `gdt_descriptor` | 16 |
| Mode switch | `switch_to_pm` | 16 |
| Exception stubs (32-bit) | `EXCEPTION_NOERR`, `EXCEPTION_ERR`, `exception_common` | 32 |
| IDT loader | `idt_load` | 32 |
| Hardware IRQ trampolines | `keyboard_handler_asm`, `timer_handler_asm` | 32 |
| Protected-mode init | `init_pm` | 32 |
| Strings (in `.text` after the stub data) | `msg_*` | 16 (string data only) |

## Globals exported to C

```asm
global start
global framebuffer_addr
global screen_width
global screen_height
global screen_pitch
global screen_bpp
```

All five VBE fields are read by `graphics.c` as `extern unsigned int`,
`extern unsigned short` (×3), and `extern unsigned char` respectively:

```c
// graphics.c
extern unsigned int  framebuffer_addr;
extern unsigned short screen_width;
extern unsigned short screen_height;
extern unsigned short screen_pitch;
extern unsigned char  screen_bpp;
```

Their declarations sit in `.data` near the top of the file:

```asm
screen_width:  dw 0
screen_height: dw 0
screen_pitch:  dw 0
screen_bpp:    db 0
```

and `framebuffer_addr` sits next to the `vbe_mode_info` scratch buffer:

```asm
vbe_mode_info: times 256 db 0
framebuffer_addr: dd 0
```

`start` is exported so the linker script's `ENTRY(start)` matches the
real entry point.

## `start` — real-mode prologue

```asm
start:
    mov si, msg_hello
    call puts

    call enable_a20
    call set_vesa_mode

    call switch_to_pm
    jmp $
```

The three calls are ordered carefully:

1. `enable_a20` — must run before any code touches addresses above
   1 MiB. Real mode without A20 wraps at 1 MiB; for us this matters
   once the VESA framebuffer address (typically well above 1 MiB in
   QEMU) is written.
2. `set_vesa_mode` — switches the video card into a known linear
   framebuffer mode and stores the four layout fields in the
   assembly globals.
3. `switch_to_pm` — installs a flat GDT, sets `CR0.PE`, and jumps
   into 32-bit code.

The trailing `jmp $` is a safety net: if the kernel ever `ret`s out
of `kernel_main`, we'd loop here forever in 32-bit protected mode.
No instruction to clear interrupts or call BIOS again is included,
because by this point we're already in protected mode (any BIOS
interrupt would triple-fault).

## `puts` — BIOS teletype

```asm
puts:
    push si
    push ax
    push bx
.loop:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0
    int 0x10
    jmp .loop
.done:
    pop bx
    pop ax
    pop si
    ret
```

`int 0x10 ah=0x0E` writes `al` to the current cursor position and
advances the cursor. Used to print the boot-time hello message and
the VESA failure message.

`print_hex16` is also defined here but is currently unused; it prints
the 16-bit value in `ax` as four hex digits. (Intended for debugging
during early bring-up.)

## `enable_a20`

```asm
enable_a20:
    in al, 0x92
    test al, 2
    jnz .after
    or al, 2
    and al, 0xFE
    out 0x92, al
.after:
    ret
```

Port `0x92` is the "system control port A" of the PS/2 controller.
Bit 1 is the A20 gate. The routine:

1. Reads the current byte.
2. If A20 is already on, leaves immediately.
3. Otherwise, sets bit 1 (A20 on) and clears bit 0 (don't reset the
   CPU) and writes it back.

This is the fast A20 path. The keyboard-controller A20 path
(`0x64` wait/command) is more compatible with old hardware, but the
PS/2 port works on everything QEMU and Bochs emulate.

## `set_vesa_mode`

```asm
set_vesa_mode:
    mov ax, 0x4F01
    mov cx, 0x118
    mov di, vbe_mode_info
    int 0x10

    mov ax, 0x4F02
    mov bx, 0x4118
    int 0x10
    cmp ax, 0x004F
    jne .vesa_failed
    ...
```

VESA BIOS Extensions (VBE) are called via `int 0x10` with `ax =
0x4Fxx`. Two calls:

1. **`ah=0x4F, al=0x01` — Get VBE Mode Info.** `cx` is the mode
   number; `es:di` points to a 256-byte buffer that gets filled in.
   The fields we care about (all offsets from the start of the
   returned structure):

   | Offset | Size | Field |
   |---|---|---|
   | 16 | u16 | pitch (bytes per scan line) |
   | 18 | u16 | width in pixels |
   | 20 | u16 | height in pixels |
   | 25 | u8  | bits per pixel |
   | 40 | u32 | physical address of the linear framebuffer |

2. **`ah=0x4F, al=0x02` — Set VBE Mode.** `bx` is the mode number
   with bit 14 (the "use linear framebuffer" bit) set — that's why
   we pass `0x4118` instead of `0x0118`. On success `ax == 0x004F`.
   Anything else is a failure.

The four/five values are stored in the assembly globals so C can
read them.

If VESA fails, the routine prints `Failed vesa connection!` via
`puts` and falls into `jmp $`. There's no recovery path — without
a framebuffer, the kernel has nothing to draw to.

## GDT

A flat-model GDT with three entries — null, code, data — built
inline in `.text`:

```asm
gdt_start:
    dq 0x0000000000000000      ; null descriptor (selector 0x00)

gdt_code:
    dw 0xFFFF                   ; limit low  (bits 0..15)
    dw 0x0000                   ; base low   (bits 0..15)
    db 0x02                     ; base mid   (bits 16..23)
    db 10011010b                ; access     (P=1, DPL=0, S=1, type=A: code, exec/read)
    db 11001111b                ; flags+limit (G=1, D/B=1, L=0, AVL=0, limit high 4 bits = F)
    db 0x00                     ; base high  (bits 24..31)

gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x02
    db 10010010b                ; access     (P=1, DPL=0, S=1, type=2: data, read/write)
    db 11001111b
    db 0x00

gdt_end:
```

`base = 0`, `limit = 0xFFFFF` with `G=1` (granularity = 4 KiB) gives
a flat 4 GiB model. `D/B = 1` on the code segment means 32-bit
operations; on the data segment it means 32-bit operands for `esp`
and upper-bound checks. Both descriptors are ring 0 (`DPL = 00`),
present (`P = 1`), and not system (`S = 1`).

The GDT pointer:

```asm
gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start + 0x20000
```

The base address has `+ 0x20000` baked in because the kernel is
loaded at physical `0x20000` by the bootloader, but the assembler
labels inside this file resolve to **offsets within the file**
(virtual `0x0` from the linker script). Without the `+ 0x20000`,
`lgdt` would load a pointer to the file's first byte — which in
memory is actually at `0x20000` — and the CPU would triple-fault as
soon as any segment register got reloaded.

## Selectors

```asm
CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start
```

`gdt_code - gdt_start` is `8` (the null descriptor is 8 bytes), and
`gdt_data - gdt_start` is `16` (two descriptors × 8 bytes). So
`CODE_SEG = 0x08` and `DATA_SEG = 0x10` — standard.

`CODE_SEG` is used in two places:

- `jmp CODE_SEG:init_pm` — the far jump that switches into 32-bit
  mode (the far jump is required because the segment changes).
- The selector is also baked into the GDT access semantics — when
  `CR0.PE` is set and a selector with `TI=0` and `RPL=0` points at
  a code descriptor, the CPU is in 32-bit mode for that segment.

`DATA_SEG` is loaded into `ds/ss/es/fs/gs` in `init_pm`.

## `switch_to_pm`

```asm
switch_to_pm:
    cli                                ; no interrupts during the switch
    lgdt [gdt_descriptor]              ; load the GDT

    mov eax, cr0
    or eax, 1                          ; set PE
    mov cr0, eax

    jmp CODE_SEG:init_pm               ; far jump — flushes the prefetch queue
```

This is the textbook real-to-protected switch:

1. `cli` — we cannot let the CPU take an interrupt while
   `cs:ip/eip` semantics are in flux.
2. `lgdt [gdt_descriptor]` — load the new GDT. After this, segment
   register caches still hold the old (real-mode) contents, but
   the GDT itself is the new one.
3. `mov cr0, eax` with bit 0 set — turn on protected mode. The
   CPU now decodes `cs:eip` (32-bit) for any selector that points
   at a 32-bit code descriptor.
4. `jmp CODE_SEG:init_pm` — the far jump has two effects: it
   flushes the prefetch queue (otherwise the CPU would still be
   decoding 16-bit instructions), and it forces the segment
   descriptor cache for `cs` to be reloaded from the new GDT.

## Exception stubs

The IDT needs a stub for every CPU exception that should be
handled. Some exceptions push an error code automatically (8, 10,
11, 12, 13, 14, 17, 30); the others need the stub to push a
placeholder so the C handler always sees a uniform stack.

Two macros generate the stubs:

```asm
%macro EXCEPTION_NOERR 1
global exception_stub_%1
exception_stub_%1:
    push dword %1
    call exception_common
%endmacro

%macro EXCEPTION_ERR 1
global exception_stub_%1
exception_stub_%1:
    push dword %1
    call exception_common
%endmacro
```

The macros look identical today, but the intent is:

- `EXCEPTION_NOERR n` — for exceptions that don't push an error
  code. The stub pushes a placeholder (`n`) so the C handler
  always has the vector number on the stack.
- `EXCEPTION_ERR n` — for exceptions that already pushed an error
  code. The stub still pushes the vector number; the CPU's error
  code is still on the stack below it.

The current set of registered vectors is `0, 1, 2, 3, 4, 5, 6, 7,
8, 13, 14` — exactly the ones that the C side installs via
`exceptions_install` in `exceptions.c`. The unused ones (9, 10, 11,
12, 15..31) are still safe — if anything fires them, the CPU
would push a bogus selector into an empty IDT slot and the next
interrupt would either deliver a #GP or a #DF.

The shared body:

```asm
exception_common:
    pusha
    mov eax, [esp + 36]     ; retrieve the vector number
    push eax
    call exception_handler_c
    add esp, 4
    popa
    add esp, 4              ; pop the placeholder vector
    iretd
```

Stack layout on entry to `exception_common`:

```
esp + 0..31 : pushed by pusha (8 dwords: eax, ecx, edx, ebx,
                              original esp, ebp, esi, edi)
esp + 32    : the vector number we pushed (for NOERR) or
              the error code (for ERR)
esp + 36    : either the error code (for NOERR) or
              whatever was on the stack before the CPU's
              exception frame
```

So for `EXCEPTION_NOERR`, `[esp + 36]` is the vector we just
pushed (8 bytes above the saved registers). For `EXCEPTION_ERR`,
it would be the next frame, but the C handler doesn't currently
look at the error code anyway — only the vector is passed.

`exception_handler_c` halts permanently (`cli; hlt` in a loop), so
the `iretd` at the bottom is dead code in practice. It's there to
keep the assembler happy and to make it possible to return from
exceptions later without re-touching the assembly.

The vector numbers passed to `exception_handler_c` match the index
in `exception_names[]` in `exceptions.c`:

| Vector | Name | Macro |
|---|---|---|
| 0 | Divide by Zero | NOERR |
| 1 | Debug | NOERR |
| 2 | NMI | NOERR |
| 3 | Breakpoint | NOERR |
| 4 | Overflow | NOERR |
| 5 | Bound Range Exceeded | NOERR |
| 6 | Invalid Opcode | NOERR |
| 7 | Device Not Available | NOERR |
| 8 | Double Fault | ERR |
| 13 | General Protection Fault | ERR |
| 14 | Page Fault | ERR |

(`exceptions.c` has string entries for 0..8 and 13..14 — slots 9,
10, 11, 12 in the table are empty strings because the
corresponding exceptions either don't exist or aren't installed.)

## `idt_load`

```asm
global idt_load
idt_load:
    mov eax, [esp+4]
    lidt [eax]
    ret
```

Standard cdecl — first argument on the stack, return address on
top. `lidt` takes a memory operand that points to a 6-byte struct
(2-byte limit + 4-byte base), and `eax` already points at the
caller's `&idtp`. The caller's `idtp` already has the `+ 0x20000`
adjustment in its base.

## Hardware ISR trampolines

```asm
extern keyboard_handler_c

global keyboard_handler_asm
keyboard_handler_asm:
    pusha
    call keyboard_handler_c
    mov al, 0x20
    out 0x20, al          ; EOI to the master PIC
    popa
    iretd
```

```asm
extern timer_handler_c

global timer_handler_asm
timer_handler_asm:
    pusha
    call timer_handler_c
    mov al, 0x20
    out 0x20, al
    popa
    iretd
```

These are the two hardware ISRs. The pattern is:

1. `pusha` — save all eight general-purpose registers. This is
   done in assembly because the C calling convention doesn't
   guarantee callee-save for the registers we need to preserve.
2. `call C_handler` — the C handler runs with the same flat
   selectors as the rest of the kernel.
3. Send EOI (`0x20` to port `0x20` — the master PIC command port).
   This **must** happen, otherwise the PIC will never raise
   another IRQ on that line. Currently both ISRs only send EOI
   to the master PIC; that works for IRQ0 (timer) and IRQ1
   (keyboard), which both live on the master. If a slave IRQ is
   ever enabled (e.g. the unmasked IRQ12 in `pic_remap`), the
   slave EOI (`out 0xA0, al; out 0x20, al`) is required.
4. `popa; iretd` — restore registers and return from the
   interrupt.

The vector numbers these land at are:

- `keyboard_handler_asm` → vector 33 (set in `kernel_main` via
  `set_idt_gate(33, ...)`)
- `timer_handler_asm` → vector 32 (set in `timer_install` via
  `set_idt_gate(32, ...)`)

The matching PIC masks happen in `pic_remap` (see
`interrupts.md`).

## `init_pm` — protected-mode setup

```asm
init_pm:
    mov ax, DATA_SEG
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov ebp, 0x70000
    mov esp, ebp

    extern kernel_main
    call kernel_main

    jmp $
```

Each of the six segment registers is loaded with the data selector
`0x10`. The far jump in `switch_to_pm` reloaded `cs` with
`CODE_SEG (0x08)`, but the data segment registers keep their
real-mode values until something explicitly reloads them — and if
you dereference any data without doing this, you get a #GP.

The stack pointer is set to `0x70000`. The assembly file's
comment explains the math: the kernel image lives at physical
`0x20000`, so when the linker says an address is at offset
`0x70000`, the actual physical address is `0x20000 + 0x70000 =
0x90000`. C code is happy with that — from the C perspective,
the stack is at "address 0x70000" — but in real physical memory
it's at `0x90000`. The choice of `0x90000` keeps the stack out
of the way of the kernel image (which is well under 256 KB) and
out of the way of the VBE framebuffer (which is typically
`0xE0000000`+ on QEMU). It also gives roughly 0x70000 - 0x20000 =
0x50000 (320 KB) of stack space, which is comfortable for the
shallow call depths in this kernel.

`call kernel_main` transfers control to the C side. If it ever
returns, `jmp $` keeps us from running off the end of the file
into garbage.

## String data

The last section of the file is plain real-mode string data. NASM
lets us keep it in the file because we're not jumping into it —
the labels are referenced only by `mov si, msg_*` and the strings
are NUL-terminated:

```asm
msg_hello:        db 'Hello world from KERNEL!', ENDL, 0
msg_vesa_failed:  db 'Failed vesa connection!', ENDL, 0
msg_width:        db 'Width: ', 0
msg_height:       db ENDL, 'Height: ', 0
msg_pitch:        db ENDL, 'Pitch: ', 0
msg_bpp:          db ENDL, 'BPP: ', 0
msg_newline:      db ENDL, 0
msg_fb:           db ENDL, 'Framebuffer: ', 0
```

Only `msg_hello` and `msg_vesa_failed` are referenced by code.
The others are leftovers from earlier debug builds and are
unused but harmless.

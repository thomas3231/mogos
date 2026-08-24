# Interrupts

This document covers the IDT, the PIC, and the IRQ vector map. The
implementation is split across three files:

- `src/kernel/main.asm` — assembly trampolines (`keyboard_handler_asm`,
  `timer_handler_asm`, `idt_load`, exception stubs).
- `src/kernel/keyboard.c` — `struct idt_entry`, `struct idt_ptr`,
  `set_idt_gate`, `idt_install`, `pic_remap`, `outb`/`inb`.
- `src/kernel/timer.c` — `timer_install` (uses `set_idt_gate`).
- `src/kernel/exceptions.c` — `exceptions_install` (uses
  `set_idt_gate`).
- `src/kernel/kernel.c` — `kernel_main` installs the keyboard
  handler at vector 33 with `set_idt_gate(33, ...)`.

## IDT layout

The IDT is a 256-entry table of 8-byte descriptors. Two structs
describe it:

```c
struct idt_entry {
    unsigned short base_low;   // offset 0,1
    unsigned short selector;   // offset 2,3
    unsigned char  always0;    // offset 4
    unsigned char  flags;      // offset 5
    unsigned short base_high;  // offset 6,7
} __attribute__((packed));

struct idt_ptr {
    unsigned short limit;      // offset 0,1
    unsigned int   base;       // offset 2..5
} __attribute__((packed));
```

The packed attribute matters — without it the compiler would
insert padding and `sizeof(struct idt_entry)` would not equal 8.

`set_idt_gate` populates one entry:

```c
void set_idt_gate(int n, unsigned int handler) {
    idt[n].base_low  = handler & 0xFFFF;
    idt[n].base_high = (handler >> 16) & 0xFFFF;
    idt[n].selector  = 0x08;     // CODE_SEG from main.asm
    idt[n].always0   = 0;
    idt[n].flags     = 0x8E;     // P=1, DPL=00, S=0, type=1110 (32-bit interrupt gate)
}
```

Flag byte `0x8E` decoded:
- `P = 1` — present
- `DPL = 00` — ring 0
- `S = 0` — this is a gate, not a code/data segment
- `type = 1110` — 32-bit interrupt gate (clears `IF` on entry, no
  parameter copy)

The selector `0x08` is `CODE_SEG` from `main.asm` — the kernel's
flat code segment. The 32-bit handler address is split into
low/high halves because the IDT entry's address field is split
that way on the wire.

## Loading the IDT

```c
void idt_install(void) {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base  = (unsigned int)&idt + 0x20000;  // + load offset
    idt_load((unsigned int)&idtp);
}
```

The `+ 0x20000` is the same correction that `gdt_descriptor` uses:
`&idt` is the in-binary offset (zero-based, because the linker
script's `. = 0x0`); the kernel image actually lives at physical
`0x20000`, so the IDTR base must be adjusted.

`idt_load` is implemented in assembly:

```asm
global idt_load
idt_load:
    mov eax, [esp+4]
    lidt [eax]
    ret
```

The cdecl ABI puts the first argument at `[esp+4]` (above the
return address at `[esp]`).

## PIC remap

Out of the box, the master PIC (at I/O ports `0x20`/`0x21`) raises
IRQ0..7 as INT 0x08..0x0F, and the slave PIC (ports `0xA0`/`0xA1`)
raises IRQ8..15 as INT 0x70..0x77. That conflicts with the CPU's
reserved exception vectors 0..31 — IRQ0 would land at INT 0x08
(double fault!). The remap moves them to INT 0x20..0x2F.

`pic_remap` is the standard ICW1..ICW4 sequence:

```c
void pic_remap(void) {
    outb(0x20, 0x11);   // ICW1: init + expect ICW4
    outb(0xA0, 0x11);
    outb(0x21, 0x20);   // ICW2 master: vector offset 0x20
    outb(0xA1, 0x28);   // ICW2 slave:  vector offset 0x28
    outb(0x21, 0x04);   // ICW3 master: slave on IRQ2
    outb(0xA1, 0x02);   // ICW3 slave:  cascade identity
    outb(0x21, 0x01);   // ICW4: 8086 mode
    outb(0xA1, 0x01);
    outb(0x21, 0xFC);   // OCW1 master: unmask IRQ0 (timer) and IRQ1 (keyboard)
    outb(0xA1, 0xEF);   // OCW1 slave:  unmask IRQ12 (mouse, bit 4)
}
```

`0xFC` is the OCW1 mask byte: bit 0 (IRQ0/timer) and bit 1
(IRQ1/keyboard) clear, the rest set. `0xEF` clears bit 4 of the
slave mask, which is IRQ12 (the PS/2 mouse). The kernel doesn't
have a mouse driver, but the bit is left clear in case it's added
later.

## IRQ vector map

| Vector | Source | Handler | Notes |
|---|---|---|---|
| 0 | CPU | `exception_stub_0` (Divide by Zero) | NOERR macro |
| 1 | CPU | `exception_stub_1` (Debug) | NOERR |
| 2 | CPU | `exception_stub_2` (NMI) | NOERR |
| 3 | CPU | `exception_stub_3` (Breakpoint) | NOERR |
| 4 | CPU | `exception_stub_4` (Overflow) | NOERR |
| 5 | CPU | `exception_stub_5` (Bound Range Exceeded) | NOERR |
| 6 | CPU | `exception_stub_6` (Invalid Opcode) | NOERR |
| 7 | CPU | `exception_stub_7` (Device Not Available) | NOERR |
| 8 | CPU | `exception_stub_8` (Double Fault) | ERR (error code pushed by CPU) |
| 9..12 | — | (uninstalled) | Would #GP on entry. |
| 13 | CPU | `exception_stub_13` (GPF) | ERR |
| 14 | CPU | `exception_stub_14` (Page Fault) | ERR |
| 15..31 | — | (uninstalled) | |
| 32 | IRQ0 (PIT) | `timer_handler_asm` | Installed in `timer_install` |
| 33 | IRQ1 (PS/2 keyboard) | `keyboard_handler_asm` | Installed in `kernel_main` |
| 34..39 | IRQ2..7 | (masked) | |
| 40..47 | IRQ8..15 | (mostly masked) | IRQ12 (mouse) is unmasked but no handler installed — would deliver a spurious interrupt to vector 44, which the IDT gate is zero. |
| 48..255 | — | (uninstalled) | |

## Hardware ISR flow

Each hardware ISR runs the same three-step pattern in
`main.asm`:

```asm
keyboard_handler_asm:
    pusha
    call keyboard_handler_c
    mov al, 0x20
    out 0x20, al       ; EOI to the master PIC
    popa
    iretd
```

`pusha` saves the eight general-purpose registers in a fixed
order: `eax, ecx, edx, ebx, original esp, ebp, esi, edi`. The
cdecl ABI would require the C handler to preserve `ebx, esi,
edi, ebp` only, but `pusha` makes the C handler free to clobber
all of them without consequences.

`out 0x20, 0x20` is the non-specific EOI for the master PIC.
Without this, the master PIC will not raise any more IRQs on
that line. The same is needed for the slave (`out 0xA0, 0x20`) if
a slave IRQ fires — but neither current handler does that, so
the slave EOI is omitted.

`iretd` is the 32-bit interrupt return. It pops `eip, cs, eflags`
(plus `esp, ss` if the `NT` flag is set) — the equivalent of
`retf` plus an `eflags` restore.

## Why there's no `lidt` before `set_idt_gate`

`idt_install` calls `idt_load` which runs `lidt`. Until `lidt`
runs, the IDT entries are just memory; no interrupt can fire
because the kernel hasn't done `sti` yet. So the order in
`kernel_main` is safe:

```c
idt_install();           // populates the table, then lidt
exceptions_install();    // fills in CPU exception gates
pic_remap();             // remaps and unmasks IRQs
set_idt_gate(33, ...);   // installs keyboard
timer_install();         // installs PIT

// ... paint screen, draw status bar ...

__asm__ volatile ("sti");  // now interrupts are enabled
```

The IDT is fully populated and loaded before `sti` enables
interrupts. There's a small window between `timer_install` and
`sti` where the PIT might be about to fire — but until `sti`, the
CPU ignores INTR.

## Why the `+ 0x20000` is needed

`gdt_descriptor.base` and `idtp.base` both need the physical
location of the table. The linker script places everything at
virtual `0x0`, so the linker thinks `&gdt_start` and `&idt` are
zero-based. The bootloader's load address is `0x20000`, so
physically the tables are at `0x20000 + 0 = 0x20000`. Without the
adjustment, `lgdt`/`lidt` would install a pointer at
`0x0`-something, which is the real-mode IVT — the CPU would
re-enter the BIOS interrupt handlers and triple-fault on the
first protected-mode interrupt.

The same `+ 0x20000` adjustment is used in `backbuffer_init`'s
allocation math (indirectly: the backbuffer is allocated via
`kmalloc` which returns physical addresses, and the framebuffer
is at `framebuffer_addr`, so the backbuffer copy in `screen_swap`
just memcpys between the two physical addresses).

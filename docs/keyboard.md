# `keyboard.c` / `keyboard.h` — PS/2 Keyboard Driver

The keyboard driver owns the IDT/PIC plumbing (because the PS/2
controller is the first hardware device the kernel supports) and
implements a small subset of scancode set 1.

The header also declares the `outb`/`inb` I/O port helpers that
every other driver (`timer.c`, `rtc.c`, `speaker.c`) reuses
rather than declaring its own.

## Public API (`keyboard.h`)

```c
void set_idt_gate(int n, unsigned int handler);
void idt_install(void);
void pic_remap(void);
void keyboard_handler_c(void);

void outb(unsigned short port, unsigned char val);
unsigned char inb(unsigned short port);

void spawn_terminal(void);
void close_active_window(void);
void toggle_maximize(void);
void kill_active_task(void);
```

The last four (`spawn_terminal`, `close_active_window`,
`toggle_maximize`, `kill_active_task`) are declared here but
have empty implementations — they are the "Ctrl+Alt+T/C/M/X"
actions that will be wired up when a window manager exists.
See "Future hooks" below.

## Globals

```c
struct idt_entry idt[256];
struct idt_ptr  idtp;

char scancode_to_ascii[128] = { ... };
int ctrl_pressed  = 0;
int alt_pressed   = 0;
int shift_pressed = 0;
```

- `idt[256]` — the 256-entry IDT. Filled by `idt_install` (zeros
  the whole thing implicitly by writing entries on demand) and
  by `set_idt_gate`. See `interrupts.md` for the layout.
- `idtp` — the 6-byte IDT pointer for `lidt`. Set in
  `idt_install`.
- `scancode_to_ascii` — 128-entry translation table from
  scancode set 1 (without release bit) to ASCII. Entries are
  `0` for keys that don't produce a character (modifier keys,
  function keys, arrow keys).
- `ctrl_pressed`, `alt_pressed`, `shift_pressed` — modifier
  state. Latched by the key handler.

## I/O port helpers

```c
void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
```

`outb` writes `val` (in `al`) to `port`. `inb` reads from
`port` into `al`. The `"Nd"` constraint tells GCC the port
number fits in the `dx` immediate range (0..255), letting the
compiler emit the single-byte `outb al, dx` / `inb al, dx`
form.

Every other driver includes `keyboard.h` to get these. There is
no `io.h`; `keyboard.h` doubles as the I/O header.

## IDT and PIC

See `interrupts.md` for the full layout. Summary:

```c
void set_idt_gate(int n, unsigned int handler) { ... }
void idt_install(void) { ... }
void pic_remap(void)   { ... }
```

`set_idt_gate` fills in `idt[n]` with a 32-bit interrupt gate
pointing at `handler`, selector `0x08` (the kernel's code
segment from `main.asm`), DPL 0, present.

`idt_install` sets `idtp.limit`, sets `idtp.base` to
`&idt + 0x20000` (the kernel load-offset adjustment), and
calls the assembly `idt_load`.

`pic_remap` does the ICW1..ICW4 dance that moves IRQ0..7 to
vectors 32..39 and IRQ8..15 to vectors 40..47. It also unmasks
IRQ0 (timer, bit 0), IRQ1 (keyboard, bit 1), and on the slave
PIC bit 4 (IRQ12, the PS/2 mouse — even though no mouse driver
is installed yet).

## Scancode set 1 translation

```c
char scancode_to_ascii[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0,' '
};
```

The indices are scancodes without the high (release) bit. The
table maps:

- Scancode 0x01 → 27 (Escape)
- Scancode 0x02 → '1', 0x0E → '=', 0x0E+1 = 0x0F → '\b'
- Scancode 0x10 → 'q' (etc.)
- Scancode 0x1C → '\n' (Enter)
- Scancode 0x39 → ' ' (Space)
- Scancode 0x0E → '\b' (Backspace)

Modifier keys (Ctrl = 0x1D, Alt = 0x38, Shift = 0x2A/0x36) and
the function/nav keys are all `0`. Shift is **not** applied
here — the table produces the unshifted character, and
`shift_pressed` is tracked but not used by the current
`keyboard_handler_c` (so pressing Shift+'a' still prints
'a'). This is a TODO.

## `keyboard_handler_c`

```c
void keyboard_handler_c(void) {
    unsigned char scancode = inb(0x60);
    int is_release = scancode & 0x80;
    unsigned char code = scancode & 0x7F;

    if (code == 0x1D) { ctrl_pressed = !is_release; return; }
    if (code == 0x38) { alt_pressed = !is_release; return; }
    if (code == 0x2A || code == 0x36) { shift_pressed = !is_release; return; }

    if (is_release) return;

    if (ctrl_pressed && alt_pressed) {
        if (code == 0x14) { spawn_terminal();   return; }  /* T */
        if (code == 0x2E) { close_active_window(); return; }  /* C */
        if (code == 0x32) { toggle_maximize();  return; }  /* M */
        if (code == 0x2D) { kill_active_task(); return; }  /* X */
    }

    char c = scancode_to_ascii[scancode];
    if (c) {
        print_char(c);
    }
}
```

The handler runs in this order:

1. Read the scancode from port `0x60` (PS/2 data port).
2. Extract the release bit (`0x80`) and the base code
   (`& 0x7F`).
3. Update modifier latches on press and release.
4. If the key was a release, return.
5. **Shortcut interception:** if Ctrl and Alt are both held,
   match the base code against the four reserved shortcuts and
   dispatch. These take precedence over normal character input.
6. Otherwise, look the scancode up in `scancode_to_ascii` and,
   if there's a non-zero result, draw the character into the
   framebuffer via `print_char`.

The scancode is read with the high bit still set for the
`scancode_to_ascii` lookup, so the table is indexed by the
**make** (press) scancode — values `0x00..0x7F` — even though
the `is_release` short-circuit above guarantees we only get
here for press events.

The handler does not consume characters from a keyboard
buffer; it pushes them straight to `print_char`. There's no
input line, no TTY, no scrollback. This is fine for a status
bar demo, less fine once a shell is added.

## Future hooks

```c
void spawn_terminal(void)      { /* TODO */ }
void close_active_window(void) { /* TODO */ }
void toggle_maximize(void)     { /* TODO */ }
void kill_active_task(void)    { /* TODO */ }
```

These four are the kernel's "global hotkey" targets. The
shortcut interception in `keyboard_handler_c` calls them based
on `Ctrl+Alt+T/C/M/X` (scancodes 0x14/0x2E/0x32/0x2D). They
are declared in `keyboard.h` so the declarations stay
near the keyboard code, but the implementations are stubs.
When a window manager / process model is added, these will
become real entry points.

## Caveats

- **No shift handling.** Pressing Shift+'a' prints 'a'.
- **No repeat.** Key autorepeat is not implemented; holding a
  key down produces one character.
- **No buffer.** Characters are echoed directly to the
  framebuffer. There's no way to read them back.
- **Modifier state isn't checked in `scancode_to_ascii`.** A
  real implementation would have a shifted variant of the
  table and pick between the two based on `shift_pressed`.
- **No numpad.** The scancodes for the numeric keypad (0x47
  onwards) are not in the table.
- **No Caps Lock.** The Caps Lock key (0x3A) toggles
  `caps_lock` somewhere else (no such variable yet), so the
  effect is missing.

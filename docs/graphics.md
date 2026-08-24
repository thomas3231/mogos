# `graphics.c` / `graphics.h` — VESA Framebuffer Driver

The graphics module owns everything that touches the linear
framebuffer: pixel writes, rectangles, a built-in 8×8 bitmap
font, a "terminal" that prints characters into the framebuffer,
and the status bar at the top of the screen.

It also implements a backbuffer — all draws go into a
`kmalloc`-allocated scratch buffer, and `screen_swap` copies it
to the actual VBE framebuffer in one pass.

## Public API (`graphics.h`)

```c
struct screen_info {
    unsigned char* framebuffer;  // pointer to VBE linear framebuffer
    unsigned short width;
    unsigned short height;
    unsigned short pitch;        // bytes per scan line
    unsigned char  bpp;          // bits per pixel (32 for mode 0x118)
};

extern struct screen_info screen;
extern unsigned char* backbuffer;

void screen_init(void);
void put_pixel(int x, int y, unsigned char r, unsigned char g, unsigned char b);
void draw_rect(int x, int y, int width, int height,
               unsigned char r, unsigned char g, unsigned char b);
void fill_screen(unsigned char r, unsigned char g, unsigned char b);
void draw_char(int x, int y, char c,
               unsigned char r, unsigned char g, unsigned char b, int scale);
void print_char(char c);
void draw_status_bar(void);
void draw_text_line(int x, int y, const char* text);
void clear_status_text_area(void);
void draw_status_text(void);
void render_status_frame(void);
void backbuffer_init(void);
void screen_swap(void);
```

## Globals

| Symbol | Type | Defined in | Purpose |
|---|---|---|---|
| `screen` | `struct screen_info` | `graphics.c` | Filled by `screen_init` from the assembly globals; consumed by every draw routine. |
| `backbuffer` | `unsigned char*` | `graphics.c` | Pointer to the in-memory staging buffer. All `put_pixel` writes land here. |
| `term_cursor_x`, `term_cursor_y` | `int` | `graphics.c` | "Terminal" cursor. Updated by `print_char`. |
| `text_scale` | `int` | `graphics.c` | Pixel size multiplier for `print_char`. Default `2` (each font cell becomes a 16×16 block). |
| `font8x8[96][8]` | `const unsigned char` | `graphics.c` | 8×8 bitmap font for ASCII 32..126 (95 glyphs). |
| `framebuffer_addr`, `screen_width`, `screen_height`, `screen_pitch`, `screen_bpp` | externs | `main.asm` | Filled by VBE probing in `set_vesa_mode`. |

## 8×8 bitmap font

```c
const unsigned char font8x8[96][8] = { ... };
```

`font8x8[c - 32][row]` is a byte whose bits describe one row of
the glyph for ASCII `c` (`c >= 32 && c <= 126`). Bit 7 is the
leftmost pixel, bit 0 is the rightmost. Glyphs for 32..126 are
included; everything else is silently rejected by `draw_char`.

The font is the same one used in the classic IBM PC code page
437 ROM font, recoded as a hand-edited 8×8 bitmap. Each glyph
takes 8 bytes; the whole table is 768 bytes (96 × 8).

## `screen_init`

```c
void screen_init(void) {
    screen.framebuffer = (unsigned char*)(framebuffer_addr - 0x20000);
    screen.width       = screen_width;
    screen.height      = screen_height;
    screen.pitch       = screen_pitch;
    screen.bpp         = screen_bpp;
}
```

Copies the VBE-discovered values out of the assembly globals
into the `screen` struct. The `- 0x20000` on the framebuffer
address is **subtracting** the kernel load offset to make
`screen.framebuffer` an offset within the kernel image; see
"Load-offset arithmetic" below.

## `put_pixel`

```c
void put_pixel(int x, int y,
               unsigned char r, unsigned char g, unsigned char b) {
    if (x < 0 || y < 0 || x >= screen.width || y >= screen.height) return;
    int bytes_per_pixel = screen.bpp / 8;
    int offset = y * screen.pitch + x * bytes_per_pixel;
    backbuffer[offset]     = b;
    backbuffer[offset + 1] = g;
    backbuffer[offset + 2] = r;
}
```

Writes one pixel to the **backbuffer** (not the actual
framebuffer). The byte order is BGRA — blue at the lowest
offset, then green, then red, with the high byte (alpha)
untouched. This is the VBE-defined 32 bpp layout for mode
`0x118`; the kernel assumes it and writes three bytes without
touching the fourth.

The bounds check is at the top — out-of-range writes are
silently dropped. There's no clipping against the status bar
either; if you `put_pixel` inside the top 18 pixels it will
overwrite the bar until the next `draw_status_bar`.

## `draw_rect`

```c
void draw_rect(int x, int y, int width, int height,
               unsigned char r, unsigned char g, unsigned char b) {
    for (int row = y; row < y + height; row++) {
        for (int col = x; col < x + width; col++) {
            put_pixel(col, row, r, g, b);
        }
    }
}
```

A simple nested loop. `put_pixel` does the per-pixel bounds
check, so `draw_rect` doesn't have to.

`draw_rect_c(x, y, w, h, color_index)` is a thin wrapper that
looks the RGB triple up in the `palette[]` table
(see `palette.md`) before calling `draw_rect`.

`fill_screen(r, g, b)` is `draw_rect(0, 0, screen.width, screen.height, r, g, b)`.

## `draw_char` — scaled glyph

```c
int text_scale = 2;

void draw_char(int x, int y, char c,
               unsigned char r, unsigned char g, unsigned char b, int scale) {
    if (c < 32 || c > 126) return;
    int index = c - 32;
    for (int row = 0; row < 8; row++) {
        unsigned char line = font8x8[index][row];
        for (int col = 0; col < 8; col++) {
            if (line & (0x80 >> col)) {
                draw_rect(x + (col * scale), y + (row * scale),
                          scale, scale, r, g, b);
            }
        }
    }
}
```

For each set bit in the glyph's 8-byte bitmap, draws a
`scale × scale` square of pixels. With `scale=1` the glyph is
the raw 8×8 bitmap; with `scale=2` (the `print_char` default)
each glyph becomes 16×16.

The `if (c < 32 || c > 126) return;` line means control
characters drawn through `draw_char` are no-ops. `print_char`
handles `\n` and `\b` explicitly.

## `print_char` — "terminal" output

```c
int term_cursor_x = 0;
int term_cursor_y = 0;

void print_char(char c) {
    int char_size = 8 * text_scale;
    if (c == '\n') {
        term_cursor_x = 0;
        term_cursor_y += char_size;
    } else if (c == '\b') {
        if (term_cursor_x >= char_size) term_cursor_x -= char_size;
        draw_rect(term_cursor_x, term_cursor_y, char_size, char_size,
                  0x10, 0x10, 0x10);
    } else {
        draw_char(term_cursor_x, term_cursor_y, c, 0xFF, 0xFF, 0xFF, text_scale);
        term_cursor_x += char_size;
        if (term_cursor_x + char_size > screen.width) {
            term_cursor_x = 0;
            term_cursor_y += char_size;
        }
    }
    if (term_cursor_y + char_size > screen.height) {
        term_cursor_y = 0;
        fill_screen(0x10, 0x10, 0x10);
    }
}
```

- `\n` advances to the next line, column 0. No carriage return
  needed.
- `\b` (backspace) erases the previous cell by painting a
  background-colored (`0x10, 0x10, 0x10`) square over the
  cursor's current position and moves the cursor back.
- Any other printable char is drawn in white at the cursor and
  the cursor advances by `char_size` (16 px by default).
- Line wrap: when the next cell would overflow past the right
  edge, wrap to column 0 of the next line.
- Screen wrap: when the next line would overflow past the
  bottom, the cursor jumps back to row 0 and the screen is
  repainted with the background color. There's no scroll —
  the text just teleports.

The keyboard handler calls `print_char` directly when a key
is pressed. Currently there is no input line editor — the
keyboard ISR doesn't read from a "current line" buffer; it just
echoes each press to the screen.

## Status bar

The status bar is a 18-pixel-tall black bar across the top of
the screen, with a 4-pixel grass-green border on the left,
right, and bottom (and the same border in green at the bottom
of the screen). The right side and left side have the same
border pattern.

`draw_status_bar`:

```c
void draw_status_bar(void) {
    const int bar_height = 18;
    const int border = 4;
    /* black bar across the top */
    draw_rect(0, 0, screen.width, bar_height,
              palette[COLOR_BLACK][0], palette[COLOR_BLACK][1], palette[COLOR_BLACK][2]);
    /* black bar across the bottom */
    draw_rect(0, screen.height - bar_height, screen.width, bar_height, ...);
    /* grass-green border at the bottom, left, right */
    draw_rect(0, screen.height - border, screen.width, border, ...);
    draw_rect(0, 0, border, screen.height, ...);
    draw_rect(screen.width - border, 0, border, screen.height, ...);
}
```

The bar's two rectangles (top and bottom) use the same 18 px
height and the same `COLOR_BLACK` palette entry, so the top and
bottom bars are visually identical. The three grass-green
`border`-wide rectangles frame the screen on the left, right,
and bottom.

`clear_status_text_area` repaints the top bar in pure
`(0, 0, 0)` and then re-runs `draw_status_bar`. This is a
heavier "clear the bar" used before redrawing status text
because the `0x10` background in the top bar from
`fill_screen` doesn't quite match the pure-black bar.

`draw_text_line(x, y, text)` draws a string at `(x, y)` in
unscaled white. It always uses `scale=1`, so the text is the
raw 8-pixel-tall font.

`draw_status_text` reads the current uptime, CPU-usage percent,
and RTC time, formats them as three short strings, and draws
them into the top bar:

| Label | Position | Source |
|---|---|---|
| `UP: HHh MMm SSs` | `(8, 4)` | `timer_get_uptime_seconds()` |
| `CPU: NNN%` | `(220, 4)` | `timer_get_cpu_usage_percent()` |
| `HH:MM:SS` | right-aligned, `(screen.width - 80, 4)` | `rtc_read(&now)` |

All three buffers are built by hand — `'0' + (n / 10)` etc. —
so leading zeros are kept and the strings are a fixed width.

`render_status_frame` is the throttled entry point called from
the main loop:

```c
void render_status_frame(void) {
    static unsigned int last_render_tick = 0;
    static unsigned int last_uptime = 0;
    static unsigned int last_cpu = 0;
    unsigned int current_tick = timer_get_ticks();
    unsigned int uptime = timer_get_uptime_seconds();
    unsigned int cpu = timer_get_cpu_usage_percent();

    if (last_render_tick == 0) {
        draw_status_bar();
        draw_status_text();
        last_render_tick = current_tick;
        last_uptime = uptime;
        last_cpu = cpu;
        return;
    }
    if (current_tick - last_render_tick < 100) return;
    if (uptime != last_uptime || cpu != last_cpu) {
        draw_status_text();
        last_uptime = uptime;
        last_cpu = cpu;
    }
    last_render_tick = current_tick;
}
```

Behaviour:

- On the first call (`last_render_tick == 0`), draw the bar and
  the initial text, set the static latches, return.
- On every subsequent call, only redraw if **either** at least
  100 ticks (1 second) have passed since the last render
  **and** either the uptime or the CPU usage changed.

The static latches make the function idempotent enough that
calling it once per loop iteration in `kernel_main` is cheap.
The 100-tick guard means we redraw at most 100 times per
second (since the PIT runs at 100 Hz).

## Backbuffer and `screen_swap`

```c
unsigned char* backbuffer;  // declared twice — once at top of file, once near the bottom

void backbuffer_init(void) {
    unsigned int buffer_size = screen.pitch * screen.height;
    backbuffer = (unsigned char*)kmalloc(buffer_size);
}

void screen_swap(void) {
    unsigned int buffer_size = screen.pitch * screen.height;
    unsigned char* src = backbuffer;
    unsigned char* dst = screen.framebuffer;
    for (unsigned int i = 0; i < buffer_size; i++) {
        dst[i] = src[i];
    }
}
```

The backbuffer is one full frame's worth of pixels:
`pitch * height` bytes (for `1024×768×32 bpp` that's
`1024 * 4 * 768 = 3 MiB`, well within the 8 MB heap).

`backbuffer_init` must be called after `heap_init` and after
`screen_init` (because `screen.pitch` and `screen.height` need
to be filled in). The order in `kernel_main` is exactly that.

`screen_swap` is a plain `memcpy` written as a byte loop
(there is no `memcpy` in freestanding C). It runs every iteration
of the main loop after `render_status_frame`. At 1024×768×32 bpp
that's 3 MiB per frame, which is fast on a modern CPU but would
be slow on a real i386; the only optimization available would be
to use `rep movsd` instead of a byte loop, but it's not on the
critical path right now.

## Load-offset arithmetic

`framebuffer_addr` (from VBE) is a physical address. So is
`screen.framebuffer` — after `screen_init` it holds
`framebuffer_addr - 0x20000`, but in `screen_swap` it's used as
the destination of the `memcpy`. That looks wrong at first
glance. The reason it works:

- The kernel is loaded at physical `0x20000`. The C code is
  linked with the linker script at virtual `0x0`, so the
  C-visible "address" of a C symbol is its offset within the
  image.
- The VBE framebuffer address (`framebuffer_addr`) is a true
  physical address — VESA reports the address the BIOS mapped
  it at, which is whatever the emulator decided.
- The backbuffer is allocated via `kmalloc`, which returns a
  physical address (the heap lives at `0x200000`+, also a true
  physical address, with no paging in play).
- So `screen.framebuffer` ends up storing
  `framebuffer_addr - 0x20000` — a fake "C address" — but
  `screen_swap` doesn't dereference that as a C pointer; it
  uses it as a flat `unsigned char*` for the byte loop. The
  `+ 0x20000` adjustment that other modules need (e.g. the IDT
  and GDT) is not applied here, because the framebuffer pointer
  is just memory in the physical address space and
  `backbuffer` is also a physical address.

In short: subtract `0x20000` only when you're forming a C-visible
"address in the kernel image" (IDT, GDT). Don't subtract it
when the value is a true physical address (framebuffer, heap,
stack).

## Pitfalls

- **Drawing without `screen_swap`.** Every draw goes to the
  backbuffer. If you forget to call `screen_swap`, the changes
  never reach the screen.
- **Re-allocating the backbuffer.** `backbuffer_init` calls
  `kmalloc` and overwrites the global pointer. If anything else
  already allocated at the same address (it won't, because the
  heap is bump-allocated), the previous allocation is leaked.
- **32 bpp assumed.** `put_pixel` writes exactly three bytes
  per pixel. If the VBE mode returned a different `bpp` (8, 16,
  24), the writes will produce garbage. The mode is hardcoded
  to `0x118` in `main.asm`, which is always 32 bpp, so this is
  safe today but worth flagging if you ever change modes.
- **Two `backbuffer` declarations.** The file has
  `unsigned char* backbuffer;` at the top (in the globals
  block) and again right before `backbuffer_init`. This is a
  harmless duplicate definition that should probably be
  consolidated.

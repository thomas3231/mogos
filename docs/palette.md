# `palette.c` / `palette.h` — Color Palette

A 16-color VGA-style palette plus one extra "grass green"
entry. Indexed by the `COLOR_*` macros in the header.

## Public API (`palette.h`)

```c
extern const unsigned char palette[17][3];

#define COLOR_BLACK        0
#define COLOR_BLUE         1
#define COLOR_GREEN        2
#define COLOR_CYAN         3
#define COLOR_RED          4
#define COLOR_MAGENTA      5
#define COLOR_BROWN        6
#define COLOR_LIGHTGRAY    7
#define COLOR_DARKGRAY     8
#define COLOR_LIGHTBLUE    9
#define COLOR_LIGHTGREEN   10
#define COLOR_LIGHTCYAN    11
#define COLOR_LIGHTRED     12
#define COLOR_LIGHTMAGENTA 13
#define COLOR_YELLOW       14
#define COLOR_WHITE        15
#define COLOR_GRASSGREEN   16
```

## Implementation

```c
const unsigned char palette[17][3] = {
    {0x00,0x00,0x00}, {0x00,0x00,0xAA}, {0x00,0xAA,0x00}, {0x00,0xAA,0xAA},
    {0xAA,0x00,0x00}, {0xAA,0x00,0xAA}, {0xAA,0x55,0x00}, {0xAA,0xAA,0xAA},
    {0x55,0x55,0x55}, {0x55,0x55,0xFF}, {0x55,0xFF,0x55}, {0x55,0xFF,0xFF},
    {0xFF,0x55,0x55}, {0xFF,0x55,0xFF}, {0xFF,0xFF,0x55}, {0xFF,0xFF,0xFF},
    {0x7C, 0xFC, 0x00},
};
```

The 16 colors are the same 16-color RGBI palette used by VGA
text mode and the original IBM 5153 monitor, in the canonical
order. The 17th entry (`COLOR_GRASSGREEN`) is the lime-green
border color used by `draw_status_bar` in `graphics.c`.

Layout is `palette[index][0..2] = {R, G, B}` — each entry is
three bytes in RGB order. Note that `graphics.c::put_pixel`
writes the bytes back as **B, G, R** (VBE's framebuffer byte
order), so callers using the palette have to be careful: the
palette stores RGB but the framebuffer wants BGR. The current
call sites (`draw_status_bar`, `draw_rect_c`) pass the
`palette[i][0..2]` values directly into `draw_rect` /
`draw_status_bar`, which then forwards them to `put_pixel` —
and `put_pixel` interprets them as BGR, **inverting the
interpretation**.

In other words: the colors in the live UI are the inverse of
what the palette "says" they are. `COLOR_BLACK` is still black
(0, 0, 0 = 0, 0, 0), but `COLOR_RED` (R=0xAA, G=0, B=0)
shows up as blue (because the framebuffer gets bytes
{0, 0, 0xAA} = R=0xAA, B=0, G=0 — no, wait, B=0 G=0 R=0xAA,
which is the correct red).

Re-reading `put_pixel`:

```c
backbuffer[offset]     = b;     // byte 0 = B
backbuffer[offset + 1] = g;     // byte 1 = G
backbuffer[offset + 2] = r;     // byte 2 = R
```

So `put_pixel(r, g, b)` is called with `(palette[i][0],
palette[i][1], palette[i][2])` = `(R, G, B)`. That writes R to
byte 2, G to byte 1, B to byte 0. Reading the framebuffer as
BGR (which is what the VBE linear framebuffer uses), the
pixel is `(B, G, R) = (palette[i][2], palette[i][1],
palette[i][0])` — i.e. the B and R are swapped from what
`palette.h` "says". So `COLOR_RED` is the wrong colour in the
UI today; it actually displays as a blue, and what should be
blue displays as red. `COLOR_BLACK`, `COLOR_WHITE`,
`COLOR_GREEN`, and the green-bordered `COLOR_GRASSGREEN`
happen to be symmetric across the B↔R swap, which is why the
status bar looks right.

This is a latent bug that hasn't been hit because the only
status-bar palette usage is `COLOR_BLACK` and
`COLOR_GRASSGREEN`, both of which are R≈G or B≈G.

## Where it's used

- `graphics.c::draw_status_bar` — uses `COLOR_BLACK` and
  `COLOR_GRASSGREEN`.
- `graphics.c::draw_rect_c` is the public wrapper for "fill a
  rectangle with palette entry N", but no other code in the
  tree calls it.

## Caveats

- **R/B swap bug** as described above. To use this palette
  correctly, the helper that reads from it should swap the
  red and blue bytes before writing to the framebuffer, or
  the palette should be stored in BGR order. Neither is done
  today.
- **`COLOR_BROWN`** is the classic 0xAA, 0x55, 0x00 dark
  yellow. With the R/B swap in effect it would render as
  teal.

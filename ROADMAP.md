# roadmap

stuff that's done, stuff that's not. updating as i go.

## done

- bootloader (FAT12, finds KERNEL.BIN, loads it, jumps to it)
- protected mode switch
- C kernel w/ cross compiler toolchain
- VESA graphics, real res/pitch queried from BIOS (not hardcoded)
- GDT/IDT/PIC set up right
- exception handlers -> blue fault screen instead of just rebooting
- heap allocator, actual free-list now (kmalloc/kfree, coalescing)
- keyboard driver + modifier keys (ctrl/alt/shift tracked)
- PIT timer -> uptime + rough cpu usage %
- RTC -> real clock, shows in status bar
- PC speaker -> beep/nosound, boot jingle
- offscreen backbuffer (no more flicker)

## dropped

- mouse. tried it, packet sync issues, wasn't worth it. going full keyboard,
  TempleOS style. rewriting anything that assumed mouse input.

## next up

- fast blitter for the backbuffer swap (rep movsd instead of byte loop)
- actual drawing primitives - lines, sprites
- better font (colors, not just white)
- storage driver - just extend the FAT12 code that already works in the
  bootloader instead of writing ATA from scratch
- keyboard-based window switching (alt+tab, no dragging)
- paging (only once i actually need it for multitasking)
- basic scheduler
- filesystem / VFS
- SNAKE (finally)
- text editor
- maybe a 3D thing later
- HolyC-style live compiler thing - way later, if ever, this is the hard one

## rules i'm trying to follow

- keyboard only. no mouse.
- each layer only talks to the layer under it
- don't build the complicated version until the simple version isn't enough

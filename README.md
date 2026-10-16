# MogOS

MogOS is a small 32-bit Ring 0 hobby OS built as a learning project. It boots
through a custom legacy-BIOS loader and has a keyboard-operated desktop with
Snake, Settings, a clock/status bar, and a text editor.

## Build and run

Build on Ubuntu or WSL. You need `make`, NASM, mtools, dosfstools, xorriso,
QEMU, and the `i686-elf` cross-compiler on your `PATH`.

```sh
make
make test
make iso
make run_iso
```

`make run_iso` uses `build/hdd.img` as a writable ATA disk. `make clean`
preserves this image, so back it up if it contains anything important.

## Controls

- Press **Enter** at the welcome screen. Select desktop apps with **Up/Down**
  and **Enter**. **Ctrl+H** returns from an app.
- In the editor: type to edit; **arrows** move; **Backspace** deletes;
  **Ctrl+S/F2** saves; **Ctrl+O/F3** loads.
- In Settings: **A** toggles green editor text, **C** toggles clock seconds,
  **B** plays the jingle.
- In Snake: **arrows** or **W/A/S/D** steer; **P** pauses; **Enter** restarts
  after a collision; **Ctrl+H** exits.
- The terminal accepts `ping`, `run snake`, `about`, `settings`, and `exit`.

## Saving and limitations

The editor saves one document of up to 2,042 characters to a fixed slot on the
attached ATA disk. This is a prototype, not a filesystem: there are no
filenames, directories, or multiple documents. The ISO is read-only, so it
cannot hold saved changes. Without an ATA disk, editing is temporary.

The ISO uses legacy-BIOS El Torito floppy emulation. It is not a USB-hybrid
image; writing it directly to a USB drive will not make it bootable. MogOS does
not yet have mouse support, networking, paging, user-mode programs, or a
scheduler.

## Tests and demo package

`make test` runs host-side checks for selected RTC, timer, ATA, editor, and
Snake behavior. Also boot the ISO in QEMU before relying on a build.

The [Phase 1 demo package](./release/core-demo-2026-10-16/) contains a tested
ISO, a separate blank 16 MiB ATA disk, checksums, and screenshots. The disk is
writable; use a copy for tests. See [PRESENTATION.md](./PRESENTATION.md) for a
short demo and verification checklist.
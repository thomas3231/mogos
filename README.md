# MogOS

MogOS is a small 32-bit Ring 0 hobby OS built as a learning project. It boots
through a custom legacy-BIOS loader and has a keyboard-operated desktop with
Snake, Settings, a clock/status bar, and a text editor.

## Build and run

Build on Ubuntu or WSL. You need `make`, GCC with 32-bit support, NASM, mtools,
dosfstools, xorriso, and QEMU on your `PATH`. If `i686-elf-gcc` and
`i686-elf-ld` are installed, the build uses them; otherwise, it falls back to
native `gcc` and `ld` with 32-bit output.

```sh
make
make test
make iso
make run_iso
```

`make run_iso` uses `build/hdd.img` as a writable ATA disk. `make clean`
preserves this image, so back it up if it contains anything important.
On WSL, `make run_iso` uses the native Windows QEMU at
`C:\Program Files\qemu\qemu-system-i386.exe` when available. This avoids a
transparent WSLg window. Override `WINDOWS_QEMU` if QEMU is installed elsewhere.
Without native Windows QEMU, it falls back to WSLg's GTK software display.

## Controls

- Press **Enter** at the welcome screen. Select desktop apps with **Up/Down**
  and **Enter**. **Ctrl+H** returns from an app.
- In the editor: type to edit; **arrows** move; **Backspace** deletes;
  **Ctrl+S/F2** saves; **Ctrl+O/F3** loads.
- In Settings: **A** toggles green editor text, **C** toggles clock seconds,
  **B** plays the jingle.
- In Snake: **arrows** or **W/A/S/D** steer; **P** pauses; **Enter** restarts
  after a collision; **Ctrl+H** exits.
- The terminal accepts `ping`, `fetch`, `run snake`, `run editor`, `about`,
  `settings`, and `exit`. `fetch` shows a text-only MogOS system summary with
  runtime details such as uptime, CPU usage, and display resolution.

The fetch summary appears automatically when you enter the desktop after boot.

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
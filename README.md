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
- In the File Explorer: **arrows** select; **Enter** opens; **Backspace** goes
  to the parent; **N** creates a `.txt` file; **D** creates a folder; **X**
  deletes the selected item after confirmation. If the disk has no MogFS
  volume, **X** offers to format only its reserved region after confirmation.
- In the editor: type to edit; **arrows** move; **Backspace** deletes;
  **Ctrl+S** opens Save As for new documents and saves the chosen file
  thereafter; **Ctrl+O** opens the file picker; **F2/F3** save/load the legacy
  single-document slot.
- In Settings: **A** toggles green editor text, **C** toggles clock seconds,
  **B** plays the jingle.
- In Snake: **arrows** or **W/A/S/D** steer; **P** pauses; **Enter** restarts
  after a collision; **Ctrl+H** exits.
- The terminal accepts `ping`, `fetch`, `run snake`, `run editor`, `about`,
  `settings`, and `exit`. `fetch` shows a text-only MogOS system summary with
  runtime details such as uptime, CPU usage, and display resolution.

The fetch summary appears automatically when you enter the desktop after boot.

## Files and storage

The File Explorer creates nested folders and `.txt` files on the attached ATA
disk. MogFS supports 32 directory entries and eight files up to 16 KiB each.
The first **Ctrl+S** in a new editor document opens Save As; later saves update
that file. **Ctrl+O** opens the file picker. **F2/F3** retain access to the old
single-document save slot. MogFS reserves LBA 16–536 and does not format
automatically; the explorer requires confirmation before initializing that
region. See [the storage format](./docs/storage-format.md).

## Saving and limitations

The editor's legacy save slot holds one document of up to 2,042 characters.
New text files use MogFS. The ISO is read-only, so it cannot hold saved
changes. Without an ATA disk, editing and file management are unavailable.

The ISO uses legacy-BIOS El Torito floppy emulation. It is not a USB-hybrid
image; writing it directly to a USB drive will not make it bootable. MogOS does
not yet have mouse support, networking, paging, user-mode programs, or a
scheduler.

## Tests and releases

`make test` runs host-side checks for the heap, RTC, timer, ATA, editor, Snake,
and MogFS format/mount, nested-path, persistence, and I/O-failure behavior.
Also boot the ISO in QEMU before relying on a build.

The source for the [MogOS v0.2.0 release](./release/mogos-v0.2.0/) is tagged
`v0.2.0`. Its package contains the current ISO, a separate blank 16 MiB ATA
disk, checksums, and release notes. Use a copy of the writable disk for tests.
The earlier
[v0.1.0 core demo package](./release/core-demo-2026-10-16/) remains available.
See [PRESENTATION.md](./PRESENTATION.md) for a short demo and verification
checklist.
# MogOS Demo Notes

## Five-minute demo

1. Boot the ISO in QEMU and press **Enter**.
2. Point out the status bar: uptime, approximate CPU busy time, and RTC clock.
3. Open **Settings**. Press **A** and **C** to change the options; **B** plays
   the jingle. Settings reset when MogOS restarts.
4. Open **Snake**. Steer with arrows or **W/A/S/D**, pause with **P**, hit a
   wall, then press **Enter** to restart. **Ctrl+H** returns to the desktop.
5. Open **File Explorer** and create a folder and `.txt` file with **D** and
   **N**. Open it, type a short line, save with **Ctrl+S**, then reopen it to
   show the saved text. **Ctrl+O** opens the editor's file picker.
6. Explain that the document is stored on the attached ATA disk, not in the
   read-only ISO. About lists the main limitations.

## Build and test

From Ubuntu or WSL, with `i686-elf-gcc`, NASM, mtools, dosfstools, xorriso, and
QEMU installed:

```sh
make test
make clean
make all iso
xorriso -indev build/mogos.iso -report_el_torito plain
```

Boot with a disposable writable disk:

```sh
cp release/core-demo-2026-10-16/demo-disk.img build/demo-test.img
qemu-system-i386 -cdrom build/mogos.iso -boot d \
  -drive file=build/demo-test.img,format=raw,if=ide,index=0,media=disk
```

Do not use `build/hdd.img` for tests if it contains data you want to keep;
`make clean` does not remove it. The ISO is legacy-BIOS El Torito floppy
emulation, read-only, and not USB-bootable.

## Checks completed 16 October 2026

- `make test` and a clean cross-compiler ISO build passed. QEMU booted both the
  rebuilt ISO and the packaged ISO.
- The editor saved and reloaded `AaBb!!` after a clean rebuild using the same
  disposable disk. Empty documents, the 2,042-character limit, and missing-disk
  save errors were checked.
- Shift/Caps Lock, Settings toggles and jingle, Snake launch/exit/pause/
  collision/restart, and the desktop status bar were checked in QEMU.
- `xorriso` confirmed the BIOS El Torito floppy-emulation entry.

`make test` covers selected RTC, timer, ATA, editor, Snake, and MogFS cases.
Screenshots from the original QEMU checks are in
`release/core-demo-2026-10-16/screenshots/`.

## MogFS reboot verification — 30 October 2026

- Built the current ISO and booted it in QEMU with a fresh test copy of the
  packaged blank demo disk; the original packaged image was left untouched.
- Formatted the MogFS region, created `/phase2/persist.txt`, and saved
  `Persistence after reboot` from the editor.
- Reset the guest, reopened the file from the remounted volume, and confirmed
  the saved text was still present.
- `make test` passed, and `xorriso` confirmed the ISO's BIOS El Torito
  floppy-emulation boot entry.

## What the demo does not have

The legacy `MOG1` save slot still stores one document up to 2,042 characters;
F2/F3 retain access to it. New text files use the nested MogFS filesystem.
The ISO is read-only. USB boot, mouse support, networking, paging, user-mode
programs, and multitasking are not implemented.

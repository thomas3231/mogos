# MogOS

A 32-bit hobby OS with a desktop, file manager, text editor, terminal, and Snake.

## Build

On Ubuntu/WSL, install `make`, GCC with 32-bit support, NASM, mtools, dosfstools,
xorriso, and QEMU, then:

```sh
make
make test
make iso
make run_iso
```

`make run_iso` uses `build/hdd.img` for writable storage; `make clean` keeps it.
The ISO itself is read-only.

## Controls

- **Enter** starts the desktop; **Up/Down** select apps.
- **Ctrl+H** goes back.
- **File Explorer:** arrows select, **Enter** opens, **Backspace** goes up,
  **N** creates a text file, **D** creates a folder, **X** deletes.
- **Editor:** type to edit, **Ctrl+S** saves, **Ctrl+O** opens a file.
- **Snake:** arrows or **W/A/S/D** steer, **P** pauses.
- **Terminal:** `ping`, `fetch`, `run snake`, `run editor`, `about`, `settings`,
  `exit`.

Files are stored on the attached ATA disk. MogFS supports 32 directory entries
and files up to 16 KiB. It only formats its reserved disk area after you confirm.
More on the format: [docs/storage-format.md](./docs/storage-format.md).

This is a BIOS-only project; no mouse or networking yet. See
[PRESENTATION.md](./PRESENTATION.md) for a quick demo.


```sh
make
make test
make iso
make run_iso
```

`make run_iso` uses `build/hdd.img` for writable storage; `make clean` keeps it.

## Controls

- **Enter** starts the desktop; **Up/Down** select apps.
- **Ctrl+H** goes back.
- **File Explorer:** arrows select, **Enter** opens, **Backspace** goes up,
  **N** creates a text file, **D** creates a folder, **X** deletes.
- **Editor:** type to edit, **Ctrl+S** saves, **Ctrl+O** opens a file.
- **Snake:** arrows or **W/A/S/D** steer, **P** pauses.
- **Terminal:** `ping`, `fetch`, `run snake`, `run editor`, `about`, `settings`,
  `exit`.

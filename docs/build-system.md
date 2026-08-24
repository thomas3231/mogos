# Build System

This document covers the Makefile, `run.sh`, the host tool, and the full
toolchain (including the cross-compiler the kernel requires).

## Top-level Makefile

The Makefile is intentionally explicit. There is **no pattern rule** and **no
auto-discovery** of source files — every `.c` and `.asm` that goes into the
kernel has its own object-file rule. Adding a new kernel module is a
two-step change: add the rule, then append the `.o` to the `kernel.bin`
link line.

### Variables

```
ASM = nasm
CC  = i686-elf-gcc
LD  = i686-elf-ld
SRC_DIR     = src
TOOLS_DIR   = tools
BUILD_DIR   = build
```

There is no `CFLAGS` variable. The flags `-m32 -ffreestanding -fno-pie
-fno-pic` are inlined into every `$(CC) ...` rule.

### Phony targets

```
.PHONY: all floppy_image kernel bootloader clean always tools_fat
```

| Target | Default for | What it does |
|---|---|---|
| `all` | (default) | Builds the floppy image **and** the host FAT tool. |
| `floppy_image` | (depends on `kernel.bin` and `bootloader.bin`) | Builds `build/main_floppy.img`. |
| `kernel` | (depends on `kernel.bin`) | Builds the flat kernel binary only. |
| `bootloader` | (depends on `bootloader.bin`) | Builds the 512-byte boot sector only. |
| `tools_fat` | (depends on `build/tools/fat`) | Builds the host FAT12 inspector. |
| `clean` | | `rm -rf build/*`. |

### Building the floppy image

```
$(BUILD_DIR)/main_floppy.img: bootloader kernel
    dd if=/dev/zero of=$(BUILD_DIR)/main_floppy.img bs=512 count=2880
    mkfs.fat -F 12 -n "NBOS" $(BUILD_DIR)/main_floppy.img
    dd if=$(BUILD_DIR)/bootloader.bin of=$(BUILD_DIR)/main_floppy.img conv=notrunc
    mcopy -i $(BUILD_DIR)/main_floppy.img $(BUILD_DIR)/kernel.bin "::kernel.bin"
    mcopy -i $(BUILD_DIR)/main_floppy.img test.txt "::test.txt"
```

Step by step:

1. `dd` writes 2880 × 512 = 1,474,560 zero bytes (the standard 1.44 MB
   floppy size).
2. `mkfs.fat -F 12 -n "NBOS"` formats the image as FAT12 with the volume
   label `NBOS`. **The `mcopy` step that follows expects the filesystem
   label to be ≤ 11 characters**; `NBOS` is fine.
3. `dd conv=notrunc` writes the 512-byte `bootloader.bin` over the
   first sector of the image, preserving everything else (`notrunc`).
4. `mcopy` copies `kernel.bin` and `test.txt` from the host filesystem
   onto the image. `mcopy` understands FAT short filenames and writes
   them into the data region; it also adds them to the FAT.

After this rule finishes, `build/main_floppy.img` is a complete,
self-contained, bootable floppy image.

### Bootloader rule

```
$(BUILD_DIR)/bootloader.bin: always
    $(ASM) $(SRC_DIR)/bootloader/boot.asm -f bin -o $(BUILD_DIR)/bootloader.bin
```

`nasm -f bin` produces a flat `.bin` from `boot.asm` — no linker, no
relocations, no ELF. The 512-byte boundary and the `0xAA55` boot
signature at the end are enforced by the file itself (`times 510-($-$$)
db 0; dw 0AA55h`).

### Kernel object rules

Each kernel `.c` has a hand-written rule, e.g.:

```
$(BUILD_DIR)/graphics.o: always $(SRC_DIR)/kernel/graphics.c
    $(CC) -m32 -ffreestanding -fno-pie -fno-pic \
          -c $(SRC_DIR)/kernel/graphics.c -o $(BUILD_DIR)/graphics.o
```

`always` is a phony target whose only action is `mkdir -p $(BUILD_DIR)`.
This pattern means **header changes are not tracked** — there is no
`%.h` dependency and no `-MMD`. Touching a header will not trigger a
rebuild. Workarounds:

- Touch the corresponding `.c` (forces a recompile of that translation
  unit).
- Or run `make clean && make kernel`.

The `kernel_entry.o` rule assembles `main.asm` into an ELF32 object so
the linker can resolve its `global` symbols (`start`,
`framebuffer_addr`, `screen_width`, etc.):

```
$(BUILD_DIR)/kernel_entry.o: always $(SRC_DIR)/kernel/main.asm
    $(ASM) $(SRC_DIR)/kernel/main.asm -f elf32 -o $(BUILD_DIR)/kernel_entry.o
```

### Kernel link rule

```
$(BUILD_DIR)/kernel.bin: always \
    $(BUILD_DIR)/kernel_entry.o \
    $(BUILD_DIR)/kernel.o \
    $(BUILD_DIR)/graphics.o \
    $(BUILD_DIR)/keyboard.o \
    $(BUILD_DIR)/heap.o \
    $(BUILD_DIR)/timer.o \
    $(BUILD_DIR)/palette.o \
    $(BUILD_DIR)/exceptions.o \
    $(BUILD_DIR)/rtc.o \
    $(BUILD_DIR)/speaker.o
    $(LD) -m elf_i386 -T $(SRC_DIR)/kernel/linker.ld -o $(BUILD_DIR)/kernel.bin \
        $(BUILD_DIR)/kernel_entry.o $(BUILD_DIR)/kernel.o \
        $(BUILD_DIR)/graphics.o $(BUILD_DIR)/keyboard.o $(BUILD_DIR)/heap.o \
        $(BUILD_DIR)/timer.o $(BUILD_DIR)/palette.o $(BUILD_DIR)/exceptions.o \
        $(BUILD_DIR)/rtc.o $(BUILD_DIR)/speaker.o \
        --oformat binary
```

Important details:

- `i686-elf-ld -m elf_i386` — the linker is told the input is ELF32, so
  it understands relocations from `kernel_entry.o` (which is ELF).
- `-T src/kernel/linker.ld` — the custom linker script that places
  `kernel_entry.o`'s `.text` first.
- `--oformat binary` — produce a flat binary, no ELF headers, no
  relocations, no symbols. After this step, **`kernel.bin` is
  undebuggable as an ELF**; you need a parallel ELF build for GDB
  symbol info, or you have to disassemble against the linker output.

### Host tool rule

```
$(BUILD_DIR)/tools/fat: always $(TOOLS_DIR)/fat/fat.c
    mkdir -p $(BUILD_DIR)/tools
    gcc -std=gnu17 -g -o $(BUILD_DIR)/tools/fat $(TOOLS_DIR)/fat/fat.c
```

This uses the **system `gcc`**, not `i686-elf-gcc`, because the FAT
inspector is a normal Linux program. See `host-tools.md` for what it
does.

### Common workflows

| Goal | Command |
|---|---|
| Build everything | `make` |
| Build only the kernel | `make kernel` |
| Build only the floppy image | `make floppy_image` |
| Build only the bootloader | `make bootloader` |
| Build only the FAT tool | `make tools_fat` |
| Build a single object | `make build/graphics.o` (then `make build/kernel.bin`) |
| Wipe build/ | `make clean` |

## `run.sh`

```
qemu-system-i386 -fda build/main_floppy.img \
                 -audiodev pa,id=snd0 \
                 -machine pcspk-audiodev=snd0
```

Runs the OS in QEMU's i386 PC emulator. Flags:

- `-fda build/main_floppy.img` — use the floppy image as a 3.5" drive.
- `-audiodev pa,id=snd0` — declare a PulseAudio backend named `snd0`.
- `-machine pcspk-audiodev=snd0` — route the i440FX PC speaker output
  through `snd0` so the 880 Hz beep in `kernel_main` is audible on
  the host. Without this, QEMU silently swallows PC speaker writes.

## Toolchain

The kernel cannot be compiled with the system's stock `gcc` because
that `gcc` is a hosted compiler — it expects a libc, a startup file,
and a sane runtime environment. The kernel needs a **freestanding**
target: no libc, no startup files, no hosted headers. The `i686-elf`
target is GNU's standard name for that bare-metal i386 cross-compiler.

### apt packages (Ubuntu / WSL)

```
sudo apt update && sudo apt upgrade -y
sudo apt install -y build-essential nasm mtools dosfstools \
                    qemu-system-x86 gdb wget nano git \
                    bison flex libgmp3-dev libmpc-dev libmpfr-dev \
                    texinfo libisl-dev
```

| Package | What it provides |
|---|---|
| `build-essential` | host `gcc`, `make`, etc. (for `tools/fat`) |
| `nasm` | Assembler for both `boot.asm` and `main.asm` |
| `mtools` | `mcopy` for writing `kernel.bin` and `test.txt` onto the FAT image |
| `dosfstools` | `mkfs.fat` for FAT12 formatting |
| `qemu-system-x86` | `qemu-system-i386` to run the OS |
| `gdb` | Optional — for kernel debugging |
| `bison`, `flex`, `libgmp3-dev`, `libmpc-dev`, `libmpfr-dev`, `texinfo`, `libisl-dev` | Build deps for compiling GCC itself |

### Cross-compiler install (from source)

```
mkdir -p ~/opt/cross-src && cd ~/opt/cross-src
export PREFIX="$HOME/opt/cross"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"

# Binutils
wget https://ftp.gnu.org/gnu/binutils/binutils-2.42.tar.gz
tar xf binutils-2.42.tar.gz
mkdir build-binutils && cd build-binutils
../binutils-2.42/configure --target=$TARGET --prefix="$PREFIX" \
    --with-sysroot --disable-nls --disable-werror
make -j$(nproc)
make install
cd ..

# GCC (C only, no headers, no libgcc by default)
wget https://ftp.gnu.org/gnu/gcc/gcc-13.2.0/gcc-13.2.0.tar.gz
tar xf gcc-13.2.0.tar.gz
mkdir build-gcc && cd build-gcc
../gcc-13.2.0/configure --target=$TARGET --prefix="$PREFIX" \
    --disable-nls --enable-languages=c --without-headers
make -j$(nproc) all-gcc
make -j$(nproc) all-target-libgcc
make install-gcc
make install-target-libgcc
```

`--without-headers` tells GCC **not** to expect any libc headers at all,
which is exactly what freestanding C wants. `libgcc` is still built
because some GCC intrinsics (32-bit shifts, 64-bit arithmetic on a
32-bit target, soft-float helpers) need it. With this done, add
`export PATH="$HOME/opt/cross/bin:$PATH"` to `~/.bashrc` and verify
with `i686-elf-gcc --version`.

### What `i686-elf-gcc` provides that system `gcc` doesn't

- Targets `i686-pc-elf` (bare-metal i386) rather than
  `x86_64-pc-linux-gnu`.
- Ships with `libgcc.a` for that target — the same code, compiled for
  the right ABI, with no glibc assumptions.
- Comes with the matching `i686-elf-ld`, `i686-elf-objdump`,
  `i686-elf-gdb`, etc. The Makefile uses `i686-elf-ld` for the
  kernel link step.

If you skip the cross-compile and use the system `gcc`, the kernel
build will fail in two predictable ways:

1. `cc1` will fail at link time when it can't find `_start` or
   `__libc_start_main` (a hosted compiler expects a libc startup
   stub; the kernel is freestanding and has neither).
2. Even with `-ffreestanding -m32`, you won't have a matching `libgcc`
   for a no-libc target, so any compiler-rt helper will be linked
   from the host libgcc with the wrong ABI assumptions.

So the cross-compiler is mandatory. The Makefile's `CC = i686-elf-gcc`
line is the one place this is enforced — no other fallback exists.

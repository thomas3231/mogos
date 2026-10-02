# mogos
## Prerequisites

MogOS is built and tested on **Ubuntu (WSL or native Linux)**.

### 1. System packages

```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y build-essential nasm mtools dosfstools qemu-system-x86 gdb wget nano git bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo libisl-dev
```

| Package | What it's for |
|---|---|
| `nasm` | Assembles the bootloader and kernel entry stub |
| `mtools` | Copies files (kernel.bin) onto the FAT12 floppy image without mounting it |
| `dosfstools` | Provides `mkfs.fat`, used to format the floppy image |
| `qemu-system-x86` | Runs/emulates the OS |
| `build-essential` | `gcc`, `make`, and friends — used to build the FAT debug tool and the cross-compiler |
| `gdb` | Optional — kernel debugging |
| `bison`, `flex`, `libgmp3-dev`, `libmpc-dev`, `libmpfr-dev`, `texinfo`, `libisl-dev` | Build dependencies required to compile the `i686-elf` cross-compiler |

### 2. Build the `i686-elf` cross-compiler

The kernel is written in freestanding C and needs a cross-compiler (not your system's regular `gcc`) that targets bare metal with no host OS assumptions.

```bash
mkdir -p ~/opt/cross-src && cd ~/opt/cross-src
export PREFIX="$HOME/opt/cross"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"

# Binutils
wget https://ftp.gnu.org/gnu/binutils/binutils-2.42.tar.gz
tar xf binutils-2.42.tar.gz
mkdir build-binutils && cd build-binutils
../binutils-2.42/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror
make -j$(nproc)
make install
cd ..

# GCC
wget https://ftp.gnu.org/gnu/gcc/gcc-13.2.0/gcc-13.2.0.tar.gz
tar xf gcc-13.2.0.tar.gz
mkdir build-gcc && cd build-gcc
../gcc-13.2.0/configure --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c --without-headers
make -j$(nproc) all-gcc
make -j$(nproc) all-target-libgcc
make install-gcc
make install-target-libgcc
```

Add it to your PATH permanently:

```bash
echo 'export PATH="$HOME/opt/cross/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
i686-elf-gcc --version
```

### 3. Clone and build MogOS

```bash
git clone https://github.com/thomastechdihs/mogos.git
cd mogos
make
```

### 4. Run it

```bash
./run.sh
```

---

**Tested on:** Ubuntu (WSL2), NASM, `i686-elf-gcc` 13.2.0, QEMU (`qemu-system-i386`)

## Saving editor documents

The text editor supports one saved document on the QEMU hard-disk image. Press **Ctrl+S** to save and **Ctrl+O** to load; the last saved document is loaded when the editor opens. The image is `build/hdd.img`, which is preserved by `make` and `make clean`. Back it up before deleting it; this early storage format is a single MogOS save slot, not a general-purpose filesystem.

# Boot MogOS from USB (Syslinux + MEMDISK)

This puts `build/main_floppy.img` on a USB stick. Syslinux starts MEMDISK,
which loads MogOS's floppy image into RAM. This is a Legacy BIOS boot method;
it does not add USB support to MogOS.

You need Ubuntu/Linux (or Ubuntu on WSL 2 with USB passthrough), a spare USB
stick, and access to a PC that supports Legacy/CSM boot.

> **Warning:** Preparing the stick erases it. Select the USB stick carefully.
> MogOS may detect internal ATA disks on the test PC. Do not save or change
> files during this smoke test; disconnect valuable internal disks if possible.

## 1. Build the MogOS floppy image

From the repository root:

```sh
make clean
make all iso
```

The file to boot is `build/main_floppy.img` (about 1.44 MB).

## 2. Install the tools

```sh
sudo apt update
sudo apt install syslinux syslinux-common dosfstools parted
```

Find the Syslinux files:

```sh
find /usr/lib/syslinux /usr/share/syslinux \
  -type f \( -name memdisk -o -name mbr.bin \) -print 2>/dev/null
```

You need one `memdisk` path and one `mbr.bin` path. If either is missing,
stop and check your Syslinux package.

## 3. Format the USB stick

Use **GParted** to prepare the spare stick:

1. Select the USB device by its model and size. **This next step erases it.**
2. Create a new **msdos** partition table.
3. Create one **FAT32 primary partition** using the space on the stick.
4. Set the partition's **boot** flag.
5. Apply the changes.

Now identify the USB device and its partition. In a terminal, run `lsblk`
before and after inserting the stick:

```sh
lsblk -o NAME,PATH,SIZE,MODEL,TRAN,TYPE,MOUNTPOINTS
```

Use the USB partition as `PART` (for example `/dev/sdb1`) and the whole USB
device as `USB` (for example `/dev/sdb`). **Do not guess these names**; check
the model and size in `lsblk`. The example paths are not necessarily correct
on your computer.

## 4. Copy the boot files and install Syslinux

Replace the example paths below with the paths you found. Run these commands
from the repository root:

```sh
PART=/dev/sdX1
USB=/dev/sdX
MEMDISK=/path/to/memdisk
MBR=/path/to/mbr.bin
```

Before continuing, verify that `PART` is the USB's FAT32 partition and `USB`
is the whole USB device:

```sh
lsblk -o NAME,PATH,SIZE,MODEL,TRAN,TYPE,MOUNTPOINTS "$USB"
```

If the desktop automatically mounted the USB partition, unmount it first using
the file manager. Then mount it and copy the files:

```sh
MOUNT=$(mktemp -d)
sudo mount "$PART" "$MOUNT"
sudo install -m 0644 "$MEMDISK" "$MOUNT/memdisk"
sudo install -m 0644 build/main_floppy.img "$MOUNT/main_floppy.img"
sudo tee "$MOUNT/syslinux.cfg" >/dev/null <<'EOF'
DEFAULT mogos
PROMPT 0
TIMEOUT 30

LABEL mogos
  LINUX /memdisk
  INITRD /main_floppy.img
EOF
sync
sudo umount "$MOUNT"
rmdir "$MOUNT"
```

Install Syslinux to the USB partition and the generic boot code to the USB's
MBR. The `dd` command writes only the first 440 bytes, preserving the
partition table:

```sh
sudo syslinux --install "$PART"
sudo dd if="$MBR" of="$USB" bs=440 count=1 conv=notrunc status=none
sync
```

The write command above targets the USB's MBR. Re-check `USB` before running
it. Never use your system drive.

## 5. Boot MogOS

1. Safely eject the USB stick.
2. On the test PC, open the one-time boot menu (often `F12`, `F11`, `F9`, or
   `Esc`).
3. Select the USB's **Legacy/BIOS** boot entry. Secure Boot must be off.
4. Syslinux should start MEMDISK, which then boots MogOS. Press Enter at the
   MogOS splash screen.
5. Check whether the display and keyboard work. Open the File Explorer and
   editor only to observe their disk messages; **do not save, delete, or
   modify files**.
6. Record the PC model, whether it booted, and any error messages. If it does
   not boot, stop and record what was on screen rather than changing the
   MogOS bootloader.

UEFI-only PCs cannot use this Legacy BIOS method. Some modern PCs may boot
MogOS but not expose their storage through its legacy ATA driver; record that
as a test result.

## Windows with WSL 2

USB drives are not automatically available inside WSL. If using WSL 2, connect
the stick with `usbipd-win` first. Follow Microsoft's
[USB passthrough instructions](https://learn.microsoft.com/en-us/windows/wsl/connect-usb),
then use `lsblk` in Ubuntu to confirm the USB device and partition appear.
If they do not appear, stop; do not guess a `/dev/sdX` path.

## Record the result

- USB model and size:
- Test PC model:
- Legacy/CSM boot entry available:
- Syslinux/MEMDISK started:
- MogOS booted:
- Display and keyboard worked:
- File Explorer disk message:
- Editor disk message:
- Errors or other notes:

## References

- [Syslinux MEMDISK documentation](https://wiki.syslinux.org/wiki/index.php?title=MEMDISK)
- [Microsoft: Connect USB devices to WSL](https://learn.microsoft.com/en-us/windows/wsl/connect-usb)

# MogOS Core Demo Package

Built and verified on 16 October 2026.

- `mogos-core-demo.iso` is the legacy-BIOS El Torito ISO, using 1.44 MiB
  floppy emulation. It is read-only and is not a USB-hybrid image.
- `demo-disk.img` is a separate, blank, 16 MiB raw ATA disk image for editor
  save/load tests. The first save writes the editor's fixed `MOG1` slot.
- `SHA256SUMS` records the SHA-256 checksums (file fingerprints) of both
  images. After downloading the images and this file into one folder, run
  `sha256sum -c SHA256SUMS` on Linux to check for accidental changes or
  corruption. The checksums do not prove who published the files.
- `screenshots/` contains QEMU verification captures.

Boot the package in QEMU from this directory:

```sh
qemu-system-i386 -cdrom mogos-core-demo.iso -boot d \
  -drive file=demo-disk.img,format=raw,if=ide,index=0,media=disk
```

Back up `demo-disk.img` before using it for important work; it is a writable
test image. Do not write the ISO directly to a USB drive and expect it to boot.

For build prerequisites, repeatable tests, the demo sequence, and known
limitations, see the repository's `README.md` and `PRESENTATION.md`.

To publish these files as a GitHub Release, create a release from a tag and
attach `mogos-core-demo.iso`, `demo-disk.img`, and `SHA256SUMS`. The tag points
to a commit; it does not create one. Make sure the tagged commit matches the
source used to build the ISO.

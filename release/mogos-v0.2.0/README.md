# MogOS v0.2.0

MogOS v0.2.0 packages the nested MogFS file explorer and editor integration,
recursive folder deletion, and the first tested heap foundation for Phase 3.
The cooperative scheduler and task model are not part of this release.

## Files

- `mogos-v0.2.0.iso` — legacy-BIOS El Torito ISO using 1.44 MiB floppy
  emulation. The ISO is read-only and is not USB-hybrid.
- `demo-disk.img` — blank 16 MiB raw ATA disk image. It is intended for
  disposable testing; copy it before use.
- `SHA256SUMS` — SHA-256 checksums for both images.

Check the images on Linux with:

```sh
sha256sum -c SHA256SUMS
```

Boot in QEMU with:

```sh
qemu-system-i386 -cdrom mogos-v0.2.0.iso -boot d \
  -drive file=demo-disk.img,format=raw,if=ide,index=0,media=disk
```

The explorer initializes the reserved MogFS area only after explicit
confirmation. Do not use a valuable disk image for filesystem experiments.
See the repository `README.md` and `PRESENTATION.md` for controls and details.

## Verification

The package was built from the v0.2.0 source checkpoint. `make test` passed,
the ISO booted to the desktop in QEMU, and `xorriso` confirmed its BIOS
El Torito floppy-emulation entry. The test suite includes heap alignment,
reuse, coalescing, invalid-free, and exhaustion checks.

# MogOS Roadmap

A practical plan for the high-school final project, starting in October 2026. Keep the graded project focused on a reliable, understandable demo. Treat USB-disk boot, a general-purpose filesystem, and a graphical file explorer as staged extensions unless the core demo is already stable.

## Current implementation and boundaries

- The BIOS boot sector loads `KERNEL.BIN` from a 1.44 MiB FAT12 floppy image, then the kernel switches into 32-bit protected mode. Kernel code runs in Ring 0; there is not yet a user-mode process model.
- `make` builds the kernel, `build/main_floppy.img`, and a separate 16 MiB ATA hard-disk image. `run.sh` boots the floppy and attaches that disk.
- `make iso` now packages the existing floppy boot image as a legacy-BIOS El Torito floppy-emulation ISO. This supports optical-media/QEMU boot; it is not a USB-hybrid ISO.
- The editor's save slot occupies fixed sectors on the ATA disk. `tools/fat/fat.c` is a host-side FAT12 reader, not a kernel filesystem.
- There is no kernel filesystem or file explorer yet. Keep the editor's existing save/load behavior working until a replacement has passed storage tests.

Do not switch this project to GRUB or claim USB boot support just to produce an `.iso`: the current kernel is a flat binary entered by a custom bootloader, not a Multiboot ELF kernel. Keep the floppy boot route reproducible while developing any alternate loader.

## October 2026: Stabilize the Core

- Keep the cross-compiler build reproducible and verify the boot flow in QEMU.
- Build the legacy-BIOS El Torito ISO with `make iso`, then verify it with QEMU using `-cdrom build/mogos.iso -boot d` and the existing `build/hdd.img`.
- Keep floppy-image boot (`./run.sh`) as the known-good fallback when validating ISO changes.
- Test keyboard input (lowercase, Shift/Caps Lock, shifted symbols, both Shift keys, and Caps Lock repeat), timer/CPU display, RTC clock, settings, Snake (including rapid direction changes), and editor save/load.
- Run `make test` for host-side RTC mode/failure, timer counter/wraparound, and Snake turn-queue regression checks.
- Fix hangs and crashes before adding major features.
- Keep the current single-document disk save as a clearly limited prototype.
- Record missing-disk and read/write failures; do not format or repurpose the save disk without a backup.

**Milestone:** a repeatable build and a demo that boots and can be operated without getting stuck.

## November 2026: Finish the Demo Experience

- Polish the editor workflow, settings, and status display.
- Check edge cases such as empty documents, maximum editor size, missing disk, and failed reads/writes.
- Remove any remaining nonfunctional UI and document known limitations.

**Milestone:** a small, coherent set of features that can be explained and demonstrated reliably.

## December 2026: Test and Prepare the Presentation

- Re-test from a clean build and confirm saved data survives a normal rebuild.
- Capture screenshots or a short backup recording of the demo.
- Write the report around the boot process, interrupts, graphics, input, and storage design.
- Decide which stretch goals to leave out; avoid starting a broad driver rewrite this late.

**Milestone:** a stable candidate build and a complete explanation of how it works.

## January 2027: Final Packaging

- Package and test both the floppy image and the legacy-BIOS El Torito ISO in QEMU.
- Treat USB-disk boot as optional unless it has its own tested hard-disk boot path. The current ISO is not USB-hybrid.
- Rehearse the presentation and keep a known-good copy of the build and demo recording.
- Make only small, tested fixes after the feature freeze.

**Milestone:** final report, reproducible build steps, and a rehearsed demo.

## Later Extension: Basic Filesystem

Keep this extension on a separate branch or behind a feature gate until the demo milestone is stable.

1. **Define storage boundaries.** Add a block-device interface above the ATA sector functions. Choose a documented disk region for filesystem metadata/data that cannot overlap the editor's current save sectors. Add explicit capacity, sector-size, and I/O error reporting.
2. **Specify a tiny on-disk format.** Start with a versioned superblock, bounded root-directory table, and a simple allocation bitmap or fixed-size data blocks. Use fixed-width fields, validate every LBA/count/name/size read from disk, and reject unknown versions rather than guessing.
3. **Test the storage layer before UI integration.** Add host-side tests with an in-memory block-device mock for format/mount, empty and full volumes, create/list/read/write/delete, multi-sector files, corrupt metadata, out-of-range requests, and failed reads/writes. Do not use the live editor save area in these tests.
4. **Integrate a text file manager.** Implement keyboard selection, open/read, create/write, and delete with visible error/status messages. Test empty directories, long names, full storage, and failed I/O.
5. **Add the desktop file explorer.** Add a menu entry and a small graphical directory/list view that uses the same file-manager API; keep rendering and keyboard navigation separate from filesystem code. Initially, support keyboard-only operation because there is no mouse driver.
6. **Move the editor only after compatibility tests.** Add named-file open/save, verify the existing prototype save is either migrated or clearly retained as a fallback, then test rebuild/reboot persistence.

Do not expand to nested directories, long filenames, caching, or concurrent access until the bounded single-directory implementation is reliable.

## Later Extension: USB-Disk Boot

USB boot is separate from optical ISO boot and requires a hard-disk-compatible image. First update the boot path to recognize a partition and use BIOS INT 13h extensions (EDD) for LBA reads; the current loader assumes a floppy layout and CHS addressing from LBA 0. Then build a partitioned USB disk image, test it in QEMU as a hard disk, and only after that document physical-device flashing with explicit device-identification and data-loss warnings. Keep the floppy and optical ISO boot paths as fallbacks.

# MogOS Roadmap

A practical plan for the high-school final project, starting in October 2026. Keep the graded project focused on a reliable, understandable demo; the filesystem is a later extension, not a requirement for the current milestone.

## October 2026: Stabilize the Core

- Keep the cross-compiler build reproducible and verify the boot flow in QEMU.
- Test keyboard input, timer/CPU display, RTC clock, settings, Snake, and editor save/load.
- Fix hangs and crashes before adding major features.
- Keep the current single-document disk save as a clearly limited prototype.

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

- Package and test the legacy-BIOS boot image in QEMU.
- Try USB boot on available hardware, with QEMU as the reliable fallback.
- Rehearse the presentation and keep a known-good copy of the build and demo recording.
- Make only small, tested fixes after the feature freeze.

**Milestone:** final report, reproducible build steps, and a rehearsed demo.

## Later Extension: Basic Filesystem

After the final-project build is stable, replace the fixed save slot with a minimal filesystem. Start with a small file table and simple allocation scheme, then add create, list, read, write, and delete operations. Connect the editor to named files only after storage reads and writes have focused tests. Keep this as a separate stretch project so it cannot destabilize the school-project demo.

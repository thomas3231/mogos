# MogOS: Six-Month Roadmap

**Planning window:** 16 October 2026 to 16 April 2027
**Goal:** Grow MogOS from a stable Ring 0 demo into a small OS with a tested
filesystem and file explorer, a deliberately bounded multitasking/memory
experiment, and a real-time 2.5D raycasting demo—without losing the known-good
bootable build.

This is an ambitious solo-project plan, not a promise that every stretch goal
will fit. Preserve a working build at every checkpoint. If a stage slips, cut
optional features before cutting testing, documentation, or the final packaging
buffer.

## Current baseline (verified 30 October 2026)

- The custom legacy-BIOS loader boots a FAT12 floppy image and loads a flat
  kernel binary. The kernel enters 32-bit protected mode and executes in Ring 0.
- The existing ISO uses El Torito floppy emulation. It boots in QEMU but is
  read-only and is **not** a USB-hybrid image.
- The desktop has keyboard input, graphics, RTC/timer status, Settings, Snake,
  and a text editor.
- The editor saves one document to a fixed `MOG1` slot on an attached ATA disk.
  A clean ISO rebuild and reboot with the same disk preserved a saved document.
- `make test` covers selected RTC, timer, ATA, editor, Snake, and MogFS
  regressions.
- Phase 1 was completed and re-verified in QEMU on 16 October 2026. The manual
  checklist, no-disk behavior, empty/full editor cases, and rebuild persistence
  passed; see [PRESENTATION.md](./PRESENTATION.md) and the packaged screenshots.
- The nested MogFS core, keyboard File Explorer, and editor file picker are
  implemented. QEMU persistence across a guest reboot was verified on
  30 October 2026 using a disposable disk. There is no mouse driver,
  networking, user-mode process model, or USB-disk boot path.
- The bootable ISO, separate blank ATA demo disk, checksums, and QA screenshots
  are in [release/core-demo-2026-10-16/](./release/core-demo-2026-10-16/).
  The final report and backup recording still need to be prepared.

## Non-negotiable working rules

1. Keep a known-good ISO and a separate backup of its writable disk image.
   Never use a valuable or personal disk image for filesystem experiments.
2. Keep changes small enough to test. Each stage ends with host tests, a clean
   cross-build, and a QEMU smoke test before the next stage begins.
3. Add host-side tests for storage formats and algorithms before wiring them
   into the desktop UI.
4. Avoid silently treating failed operations as success. Show useful UI errors
   and preserve existing data on failed reads or writes.
5. Keep the existing editor save slot compatible until the filesystem-backed
   editor has passed round-trip and migration tests.
6. No broad rewrite during the last two weeks. USB boot is optional and must
   not displace the core demo or final rehearsal.

## Completed — Phase 1: Core freeze and polish
### Completed 16 October 2026

- `make test` passed; `make clean && make all iso` rebuilt the kernel and
  legacy-BIOS El Torito ISO from scratch.
- QEMU booted the ISO repeatedly, including the packaged ISO with its blank
  writable ATA disk. `xorriso` confirmed the BIOS floppy-emulation boot entry.
- The editor saved a modifier-test document, then loaded it after another clean
  rebuild using the same disposable disk. Empty-document save/load, the
  2,042-character limit and recovery, and the explicit missing-disk save error
  were also checked.
- QEMU manual checks covered Shift/Caps Lock, Settings toggles and captured
  jingle output, Snake launch/exit/pause/collision/restart, and the
  uptime/CPU/RTC status bar.
- No reproducible core defect was found during this pass. The ATA `MOG1`
  format remains a fixed save slot, not a filesystem.
- The standalone package and screenshots are archived in
  [release/core-demo-2026-10-16/](./release/core-demo-2026-10-16/).

**Exit gate: passed.** The existing demo is ready to serve as the known-good
baseline for the next stage. Filesystem, paging, and USB boot were not started.

## Completed — Phase 2: Minimal filesystem and keyboard-first file explorer
### Completed 30 October 2026

The MogFS block-device layer, bounded nested filesystem, keyboard-first File
Explorer, and filename-based editor open/save are implemented. Host stability
tests cover filesystem operations, errors, and storage edge cases.

QEMU verification used a fresh copy of the packaged blank demo disk: created
`/phase2/persist.txt`, saved `Persistence after reboot`, reset the guest, and
reopened the file from the remounted volume with the same text. The ISO booted
through its BIOS El Torito floppy-emulation entry, and `make test` passed.

**Exit gate: passed.** The writable MogFS volume is separate from the legacy
editor save slot; unsupported or corrupt volumes are not formatted
automatically. Deleting a folder removes its nested contents.

**Stretch only:** Mouse support. A complete graphical mouse/windowing stack is
not a prerequisite for a useful file explorer.

## Phase 3 — Memory foundations and a bounded task model
### 17 December 2026–16 January 2027

**Goal:** Learn and demonstrate scheduling and memory ownership without turning
the school-project kernel into an unbounded process rewrite.

- Audit the current heap: allocation boundaries, alignment, exhaustion
  behavior, and testability. Add deterministic allocator tests and fail
  visibly/safely on exhaustion.
- Define a small task structure and explicit task states, stack ownership, and
  saved CPU context. Keep the first implementation cooperative.
- Demonstrate two simple kernel tasks (for example, a clock/status updater and
  a background counter) while ensuring keyboard input and Snake remain
  responsive.
- Only consider timer-driven preemption after context switching, stack
  boundaries, interrupt return state, and task teardown are independently
  tested.
- Treat paging and user-mode isolation as a stretch. If attempted, start with a
  documented page-table model and fault tests; do not claim process isolation
  unless user/supervisor permissions and fault handling are demonstrated.

#### Work started 30 October 2026

- Added a host-testable heap-region initializer and deterministic tests for
  8-byte alignment, zero-size requests, reuse, invalid/double frees,
  coalescing, and exhaustion.
- Hardened allocation size rounding and capacity checks against unsigned
  overflow. `kmalloc` reports exhaustion by returning null; `kfree` ignores
  pointers that are not live allocation starts.

**Stopping point for this session:** allocator foundation and host tests only.
No task model, context switching, or scheduler wiring has started. Commit this
checkpoint before moving to task-state and stack-ownership design.

**Exit gate:** The cooperative scheduler has repeatable task-switch tests,
bounded stacks, no corruption of the existing desktop, and a documented
recovery/failure policy.

**Cut line:** If task switching threatens the stable boot or storage features,
keep the allocator improvements and postpone scheduling.

## Phase 4 — Feature freeze, packaging, and presentation
### 2–17 March 2027

**Goal:** Deliver one repeatable, explainable build.

- Freeze features. Fix only small, reproducible defects.
- From a clean build, run `make test`, build the ISO, verify its El Torito boot
  record, boot in QEMU, and rehearse the complete demo.
- Re-test filesystem/editor persistence on a disposable disk and confirm the
  known-good disk backup is readable.
- Capture clear screenshots and a short backup recording. Keep copies outside
  `build/`.
- Finish the report with architecture diagrams or annotated flow sketches for
  boot, interrupts, graphics/input, storage, scheduler/memory (if implemented),
  and raycasting.
- Rehearse a short presentation with an offline QEMU fallback and a saved
  recording.

**Exit gate:** The clean build and test suite pass, the demo can be operated
without getting stuck, limitations are stated honestly, and all presentation
assets exist in at least two safe locations.

**Schedule buffer:** 18 March–16 April 2027 is reserved for school deadlines,
repeat rehearsals, verified backups, and small fixes. Do not spend it on new
features unless every exit gate is already satisfied.

## USB-disk boot — needed for end of the project!!!

The current ISO boots as legacy-BIOS optical media through floppy emulation.
Writing it directly to a USB drive does **not** make the present boot path
USB-bootable. A real USB-disk path requires a disk-style image/boot sector,
partition-aware addressing, and BIOS INT 13h extensions (EDD) or another
explicitly tested loader path.

Do not flash a physical drive until a partitioned image has first booted as a
hard disk in QEMU and the target device has been positively identified. USB
boot remains optional; the ISO/QEMU demo is the reliable fallback.

# MogOS: Roadmap (revised 1 November 2026)

**Planning window:** 1 November 2026 to 16 April 2027
**Real deadline / defense date:** ______ (fill in when confirmed; dates below
are planning targets until then)

**Goal:** Grow MogOS from a stable Ring 0 demo into a small OS with a tested
filesystem and file explorer, a bounded multitasking experiment, one showcase
feature, and a demo that also runs on real hardware from a USB stick, all
without losing the known-good bootable build.

This is an ambitious solo-project plan, not a promise that every stretch goal
will fit. Preserve a working build at every checkpoint. If a stage slips, cut
optional features before cutting testing or the packaging buffer.

## Status at a glance

| Phase | Planned | Status |
|---|---|---|
| 1. Core freeze and polish | To 16 Oct | Done |
| 2. MogFS filesystem and File Explorer | To 16 Dec | Done 30 Oct (six weeks early) |
| 3. Memory foundations and cooperative tasks | 2 Nov–12 Dec | Started (allocator and scheduler implemented; QEMU smoke passed) |
| 3b. First real-hardware smoke test | This week | Not started; no code changes |
| 4. Real-hardware readiness and USB boot | 13 Dec–24 Jan | Not started |
| 5. Shell and filesystem integration, one showcase | 25 Jan–28 Feb | Not started |
| 6. Feature freeze, packaging, and presentation | 1–17 Mar | Not started |
| Schedule buffer | 18 Mar–16 Apr | Reserved |

## Current baseline (verified 30 October 2026)

- The custom legacy-BIOS loader boots a FAT12 floppy image and loads a flat
  kernel binary. The kernel enters 32-bit protected mode and executes in Ring 0.
- The existing ISO uses El Torito floppy emulation. It boots in QEMU but is
  read-only and is **not** a USB-hybrid image.
- The desktop has keyboard input, graphics, RTC/timer status, Settings, Snake,
  and a text editor.
- The editor saves one document to a fixed `MOG1` slot on an attached ATA disk.
- `make test` covers selected RTC, timer, ATA, editor, Snake, MogFS, and
  allocator regressions.
- The nested MogFS core, keyboard File Explorer, and editor file picker are
  implemented. QEMU persistence across a guest reboot was verified on
  30 October 2026 using a disposable disk.
- There is no mouse driver, networking, user-mode process model, or USB-disk
  boot path.
- The bootable ISO, separate blank ATA demo disk, checksums, and QA screenshots
  are in [release/core-demo-2026-10-16/](./release/core-demo-2026-10-16/).

## Non-negotiable working rules

1. Keep a known-good ISO and a separate backup of its writable disk image.
   Never use a valuable or personal disk image for filesystem experiments.
2. Keep changes small enough to test. Each stage ends with host tests, a clean
   cross-build, and a QEMU smoke test before the next stage begins.
3. Add host-side tests for storage formats and algorithms before wiring them
   into the desktop UI.
4. Avoid silently treating failed operations as success. Show useful UI errors
   and preserve existing data on failed reads or writes.
5. Keep the existing editor save slot compatible until it has been explicitly
   retired and documented.
6. No broad rewrite during the last two weeks.
7. Every feature claim in the presentation must be backed by a test,
   screenshot, or live demo. Do not present features that are only planned.
8. Commit and push at the end of every session. Keep a backup copy of the
   repository outside the laptop.

## Completed: Phase 1 (core freeze and polish), 16 October 2026

- `make test` passed; `make clean && make all iso` rebuilt the kernel and
  legacy-BIOS El Torito ISO from scratch. QEMU booted the ISO repeatedly.
- Editor save/load, empty and full documents, the missing-disk error, Shift
  and Caps Lock, Settings, Snake, and the status bar were all checked.
- The package and screenshots are archived in
  [release/core-demo-2026-10-16/](./release/core-demo-2026-10-16/).

## Completed: Phase 2 (MogFS and File Explorer), 30 October 2026

The MogFS block-device layer, bounded nested filesystem, keyboard-first File
Explorer, and filename-based editor open/save are implemented. Host tests
cover filesystem operations, errors, and storage edge cases. QEMU persistence
was verified by creating `/phase2/persist.txt`, resetting the guest, and
reopening the file.

The writable MogFS volume is separate from the legacy editor save slot.
Unsupported or corrupt volumes are not formatted automatically. Deleting a
folder removes its nested contents.

## Phase 3: Memory foundations and a bounded task model

### 2 November to 12 December 2026 (moved earlier; Phase 2 finished early)

**Goal:** Learn and demonstrate scheduling and memory ownership without
turning the kernel into an unbounded process rewrite.

- **Done (30 Oct):** host-testable heap-region initializer; deterministic
  tests for alignment, zero-size requests, reuse, invalid and double frees,
  coalescing, and exhaustion; overflow-safe size checks.
- **Implemented (1 Nov):** cooperative round-robin tasks with heap-owned
  8 KiB stacks, saved CPU context, explicit states, and cleanup when an entry
  function returns. A boot-time self-test switches between two tasks twice
  and checks that a returning task's stack is reclaimed; host tests cover
  ready-task selection and initial stack construction.
  QEMU reached the desktop and launched Snake with the scheduler active.
- **Done (1 Nov):** Define a small task structure with explicit states, stack
  ownership, and saved CPU context. Keep the first implementation cooperative
  (`yield()`).
- **Done (1 Nov):** Demonstrate two simple kernel tasks (for example a status
  updater and a background counter) while keyboard input and Snake remain
  responsive.
- Only consider timer-driven preemption after context switching, stack
  boundaries, interrupt return state, and task teardown are independently
  tested.
- Treat paging and user-mode isolation as a stretch. Do not claim process
  isolation unless user/supervisor permissions and fault handling are
  demonstrated.

**Exit gate:** The cooperative scheduler has repeatable task-switch tests,
bounded stacks, no corruption of the existing desktop, and a documented
recovery/failure policy.

**Cut line:** If task switching threatens the stable boot or storage features,
keep the allocator improvements and postpone scheduling.

## Phase 3b: First real-hardware smoke test

### One evening, this week — no code changes

USB boot is required for the end of the project, and real hardware is where
unknown problems hide. Find out early what actually breaks.

- Put `build/main_floppy.img` on a spare USB stick using Syslinux and MEMDISK
  (follow the step-by-step
  [Syslinux + MEMDISK smoke-test guide](./docs/usb-memdisk-smoke-test.md)).
- Boot an older PC or laptop in Legacy/CSM mode with Secure Boot off.
- Record whether it boots, whether video and keyboard work, and what the
  editor and File Explorer report about the disk. Do not fix anything during
  this test.
- Use the result to decide how much work Phase 4 needs.

## Phase 4: Real-hardware readiness and USB boot

### 13 December 2026 to 24 January 2027

**Goal:** The demo runs on a real machine from a USB stick, and failures are
handled instead of crashing.

- **Video fallback:** List the available VBE modes (functions `0x4F00` and
  `0x4F01`) and pick a supported 24/32-bit linear-framebuffer mode instead of
  hard-coding `0x118`. Keep the current mode as the first choice.
- **Storage fallback:** Add a RAM-disk block device behind the existing MogFS
  block interface, used when no ATA disk answers. Format it in memory at boot
  and clearly mark it as volatile. Many modern PCs use SATA in AHCI mode, so
  the legacy ATA ports the driver uses may not exist; without this fallback,
  the File Explorer and editor would show "no disk" on real hardware.
- **Robustness:** Install a default handler for every IDT entry and all 32 CPU
  exception vectors (currently only some are wired), and handle spurious IRQs,
  so an unexpected event shows the fault screen instead of silently rebooting.
- **Memory size:** Read the BIOS memory map (`INT 15h`, E820) in real mode and
  pass the usable RAM size to the kernel; show it in the status/fetch screen.
  Refuse to start with a clear message if there is too little RAM.
- **USB boot, tier 1 (required):** The
  [Syslinux + MEMDISK smoke-test guide](./docs/usb-memdisk-smoke-test.md) is
  documented. Repeat it from scratch to confirm it works.
- **USB boot, tier 2 (only if everything else is done):** Build a disk-style
  image with an MBR and BIOS `INT 13h` extended reads. It must boot as a hard
  disk in QEMU before any physical drive is flashed, and the target device
  must be positively identified before writing.

**Exit gate:** The demo boots from USB on at least one real machine, or the
failure is understood and documented and the VM demo is the primary plan.
QEMU behavior is unchanged and `make test` passes.

## Phase 5: Shell and filesystem integration, plus one showcase

### 25 January to 28 February 2027

**Goal:** Make the existing pieces feel like one operating system, then add
one impressive feature.

- **Shell commands on MogFS:** `ls`, `cat`, `touch`, `mkdir`, `rm`, `pwd`,
  `cd`, and `echo text > file`, plus command history on the Up arrow.
  Host-test the path and argument parsing.
- **Showcase: pick exactly one and time-box it to three weeks.**
  - **A. Program loader:** Load a flat binary from MogFS into memory and run it
    through a small table of kernel functions (print, draw, read key, exit).
    This demonstrates loading and running simple programs.
  - **B. 2.5D raycasting demo:** Implement DDA grid traversal as host-tested C,
    fixed-point math with no floating point, and a handful of columns first.
    Add keyboard movement, collision, and a clear exit key for a visual demo.
- **Stretch only:** Textures and sprites (`.mgs` already exists), multiple
  maps, mouse support, and sound effects in Snake.

**Exit gate:** Shell file commands work in QEMU and from USB (with RAM disk if
needed), the chosen showcase runs with a reliable exit, and the desktop,
editor, and Snake can still be reached and tested.

## Phase 6: Feature freeze, packaging, and presentation

### 1 to 17 March 2027

**Goal:** Deliver one repeatable, explainable build.

- Freeze features. Fix only small, reproducible defects.
- From a clean build, run `make test`, build the ISO, verify its El Torito boot
  record, boot in QEMU, and rehearse the complete demo.
- Prepare **two** USB sticks, boot both on the target machine type, and keep
  one as a spare.
- Re-test filesystem/editor persistence on a disposable disk and confirm the
  known-good disk backup is readable.
- Capture clear screenshots and a short backup recording. Keep copies outside
  `build/` and in at least two safe locations.
- Rehearse the presentation three times, once without notes, with an offline
  QEMU fallback and the saved recording.

**Exit gate:** The clean build and test suite pass, the demo can be operated
without getting stuck, limitations are stated honestly, and all presentation
assets exist in at least two safe locations.

**Schedule buffer:** 18 March to 16 April 2027 is reserved for school
deadlines, repeat rehearsals, verified backups, and small fixes. Do not spend
it on new features unless every exit gate is already satisfied.

## USB-disk boot: required, time-boxed, with fallback

The current ISO boots as legacy-BIOS optical media through floppy emulation.
Writing it directly to a USB drive with Rufus does **not** work.

- **Tier 1 (required, Phase 3b and 4):** Syslinux + MEMDISK loads
  `main_floppy.img` into RAM and presents it to the bootloader as a floppy,
  so no boot-path code changes are needed. Follow the
  [step-by-step smoke-test guide](./docs/usb-memdisk-smoke-test.md). Not yet
  verified on real hardware.
- **Tier 2 (optional):** A true disk-style image with MBR and `INT 13h`
  extensions, booted as a hard disk in QEMU first.
- **Limits:** UEFI-only PCs will not boot it (Legacy/CSM is needed), USB
  keyboards depend on BIOS legacy support, and persistent storage needs an
  ATA disk. The RAM-disk fallback is planned for Phase 4 and is not yet
  implemented.
- **Fallback:** The QEMU/VM demo is the primary plan unless the real-hardware
  run has been verified twice on the target machine.
- Do not flash a physical drive until the target device has been positively
  identified.

## Out of scope

Networking, USB drivers, multi-core, UEFI boot, user-mode processes and paging
(unless attempted and demonstrated), and a graphical windowing system.

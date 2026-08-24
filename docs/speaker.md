# `speaker.c` / `speaker.h` — PC Speaker

Drives the PC speaker via PIT channel 2. Two entry points:
`beep(frequency)` to start a tone, `nosound()` to stop it.

`kernel_main` uses `beep(880)` + `timer_wait(15)` + `nosound()`
to play a short A5 (880 Hz) beep at boot.

## Public API (`speaker.h`)

```c
void beep(unsigned int frequency);
void nosound(void);
```

## `beep`

```c
void beep(unsigned int frequency) {
    if (frequency == 0) { nosound(); return; }

    unsigned int divisor = 1193182 / frequency;
    outb(0x43, 0xB6);
    outb(0x42, divisor & 0xFF);
    outb(0x42, (divisor >> 8) & 0xFF);

    unsigned char tmp = inb(0x61);
    if (tmp != (tmp | 3)) {
        outb(0x61, tmp | 3);
    }
}
```

PIT channel 2 is connected to the PC speaker. The control flow:

1. **Guard against zero frequency.** A divisor of zero is
   undefined behaviour for the PIT; `nosound` is the safe
   alternative.
2. **Compute the divisor.** Same crystal as channel 0
   (1,193,182 Hz), so `divisor = 1193182 / frequency` gives
   the period of the square wave in PIT clock ticks.
3. **Program PIT channel 2.** `outb(0x43, 0xB6)` is the
   command byte: channel 2, low/high access, mode 3 (square
   wave), binary. The two `outb(0x42, ...)` calls write the
   low and high bytes of the divisor.
4. **Gate the speaker on.** Port `0x61` is the system control
   port B. Bit 0 enables PIT channel 2 to the speaker, bit 1
   enables the speaker amplifier. Both must be set. The
   `if (tmp != (tmp | 3))` check skips the write if both bits
   are already set — a small optimization to avoid the I/O
   port write.

The frequency is `unsigned int`, so up to ~4 GHz. The PIT's
actual range is limited by the divisor: a 16-bit divisor means
frequencies down to ~18 Hz, and a divisor of 0 (no prescaling)
gives the PIT crystal itself (~1.19 MHz, well above human
hearing and probably outside the PC speaker's physical range).

## `nosound`

```c
void nosound(void) {
    unsigned char tmp = inb(0x61) & 0xFC;
    outb(0x61, tmp);
}
```

Reads the system control port, clears bits 0 and 1 (the
speaker-enable bits), and writes it back. This silences the
speaker without disturbing the other bits (which include the
keyboard clock gate, the I/O port 0x61 secondary features,
etc.).

## How the speaker makes sound

The PC speaker is a small magnetic transducer driven by PIT
channel 2. When the channel is configured in mode 3 (square
wave) and the speaker gate is open, the speaker's cone moves
at the channel's frequency. The square wave isn't a beautiful
sine wave, but the speaker is small enough that the result is
roughly a tone.

Volume control is essentially binary: gate on or gate off.
PWM-style "soft" volume would require either an external
amplifier or a different sound path (which is why QEMU needs
`-machine pcspk-audiodev=snd0` to make the sound audible on
the host).

## Caveats

- **No duration parameter.** `beep` starts a tone; the caller
  has to `timer_wait` (or otherwise sleep) and then call
  `nosound`. There is no built-in "beep for N milliseconds".
- **No volume control.** Hardware is binary.
- **The QEMU audio flag matters.** Without
  `-machine pcspk-audiodev=snd0` (or an equivalent), QEMU
  silently discards PC speaker writes. The `run.sh` script
  already includes the flag.
- **PIT channel 2 is shared.** This driver doesn't conflict
  with anything else because nothing else uses channel 2. If
  a future feature wants channel 2 for something else (e.g.
  a one-shot timer), the two will need to coordinate.

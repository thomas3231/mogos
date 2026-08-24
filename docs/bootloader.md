# Bootloader (`src/bootloader/boot.asm`)

A 512-byte 16-bit real-mode FAT12 boot sector. The BIOS loads it at
physical `0x7C00` and jumps to it; the bootloader is responsible for
locating `KERNEL.BIN` on the floppy, reading it into memory, and
transferring control to it.

## File layout

```
org 0x7C00          ; tell NASM where we are loaded
bits 16

; --- FAT12 BPB (offset 0x00..0x3E) ---
jmp short start
nop
bdb_oem:                  db 'MSWIN4.1'
bdb_bytes_per_sector:     dw 512
bdb_sectors_per_cluster:  db 1
bdb_reserved_sectors:     dw 1
bdb_fat_count:            db 2
bdb_dir_entries_count:    dw 0E0h
bdb_total_sectors:        dw 2880
bdb_media_descriptor_type:db 0F0h
bdb_sectors_per_fat:      dw 9
bdb_sectors_per_track:    dw 18
bdb_heads:                dw 2
bdb_hidden_sectors:       dd 0
bdb_large_sector_count:   dd 0

; --- Extended Boot Record (offset 0x24..0x3E) ---
ebr_drive_number:         db 0
                          db 0            ; reserved
ebr_signature:            db 29h
ebr_volume_id:            db 12h,34h,56h,78h
ebr_volume_label:         db 'NANOBYTE OS'
ebr_system_id:            db 'FAT12   '

; --- Code (offset 0x3E..0x1FE) ---
start:
    ; ... (see below)
times 510-($-$$) db 0
dw 0AA55h                ; boot signature (offset 0x1FE..0x1FF)
buffer:                  ; used for FAT / root-dir sector reads
```

`org 0x7C00` is **only an address hint for NASM** — it affects how labels
resolve but does not produce any bytes. The boot sector always starts at
the first byte of the file. The `0xAA55` magic at offset 510/511 is what
the BIOS checks to decide whether the sector is bootable.

## BPB (BIOS Parameter Block)

The BPB occupies the first 62 bytes of the boot sector. It is laid out
to be a standard FAT12 BPB, so the same sector can be used both as a
boot sector and as a parsable FAT header (e.g. by the `tools/fat`
inspector).

Field-by-field:

| Label | Value | Notes |
|---|---|---|
| `bdb_oem` | `'MSWIN4.1'` | Cosmetic; some boot loaders use it to choose behaviour. |
| `bdb_bytes_per_sector` | `512` | Standard. |
| `bdb_sectors_per_cluster` | `1` | 512 B cluster size. Keeps the FAT12 cluster math simple. |
| `bdb_reserved_sectors` | `1` | Sector 0 is the boot sector itself; the first FAT starts at sector 1. |
| `bdb_fat_count` | `2` | Two FATs, the standard FAT12 layout. |
| `bdb_dir_entries_count` | `0xE0` = 224 | 224 × 32 B = 7168 B = 14 sectors. |
| `bdb_total_sectors` | `2880` | 1.44 MB floppy. |
| `bdb_media_descriptor_type` | `0xF0` | 3.5" floppy. |
| `bdb_sectors_per_fat` | `9` | 9 × 512 = 4608 B for each FAT. |
| `bdb_sectors_per_track` | `18` | Overwritten by `int 13h ah=08h` at runtime. |
| `bdb_heads` | `2` | Overwritten at runtime. |
| `bdb_hidden_sectors` | `0` | Floppies have no hidden sectors. |
| `bdb_large_sector_count` | `0` | Total fits in 16 bits. |

EBR fields are mostly informational; the volume label `NANOBYTE OS` is
the only one any human will see.

## Setup (real-mode prologue)

```asm
start:
    mov ax, 0
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00              ; stack grows down from our load address
```

Data segments are zeroed, and the stack is set to grow down from
`0x7C00` (the boot sector itself). This is the maximum amount of stack
we can have without overwriting our own code, and it's enough for the
deepest call chain in the bootloader (`lba_to_chs` → `disk_read` →
`disk_reset`).

Some BIOSes actually jump to `07C0:0000` rather than the
flat-address `0000:7C00`. The next three instructions fix that case by
forcing a far return into the flat address:

```asm
push es
push word .after
retf
```

The `retf` pops `cs:ip` off the stack; with `es=0` it becomes
`0:.after`, which is `0000:7C3E` (the byte after the BPB / EBR). After
this, both the segment register and the instruction pointer are
guaranteed to point into the boot sector regardless of which entry
form the BIOS used.

## Drive number and `int 13h` geometry probe

The BIOS passes the boot drive number in `dl` (0x00 for floppy A:,
0x80 for HDD). The bootloader saves it for later use and also lets the
BIOS tell us the real geometry of the inserted disk:

```asm
mov [ebr_drive_number], dl       ; save for later disk_read calls

mov ah, 08h
int 13h                          ; returns CHS geometry in CL/DH
jc floppy_error

and cl, 0x3F                     ; mask off the top 2 bits (high cyl)
xor ch, ch
mov [bdb_sectors_per_track], cx
inc dh                           ; heads are 0-based in the BIOS result
mov [bdb_heads], dh
```

`int 13h ah=08h` returns:
- `cl[0..5]` sectors per track,
- `cl[6..7]` and `ch` together form the high bits of the max cylinder,
- `dh` max head number (0-based),
- `dl` number of drives.

We only care about sectors-per-track and head count, so we mask
accordingly and store them into the BPB. This means the BPB
declaration at the top of the file isn't authoritative — the runtime
values overwrite it. This matters because the floppy used by QEMU is
emulated with the standard 18/2 geometry, but the bootloader's
lba→chs math is correct for any geometry the BIOS reports.

## Reading the root directory

The root directory lives at LBA `reserved + fat_count * sectors_per_fat`.
For our layout that's `1 + 2*9 = 19`. The size in sectors is
`ceil(32 * dir_entries / bytes_per_sector)`:

```asm
mov ax, [bdb_sectors_per_fat]
mov bl, [bdb_fat_count]
xor bh, bh
mul bx                          ; ax = 18
add ax, [bdb_reserved_sectors]  ; ax = 19
push ax

mov ax, [bdb_dir_entries_count]
shl ax, 5                       ; ax *= 32  (224 * 32 = 7168)
xor dx, dx
div word [bdb_bytes_per_sector] ; ax = 14
test dx, dx
jnz .add_sector                 ; remainder > 0 → +1 sector
jmp .read_dir
.add_sector:
inc ax

.read_dir:
mov cl, al                      ; cl = 14
pop ax                          ; ax = LBA 19
mov dl, [ebr_drive_number]
mov bx, buffer
call disk_read                  ; reads 14 sectors into es:bx (buffer)
```

`disk_read` is described below. The `buffer` label is at the end of
the boot sector (offset `0x7E00` in memory). 14 sectors = 7168 bytes
fits comfortably in the 0x7E00..0x1FFFF free region.

## Searching for `KERNEL.BIN`

```asm
xor bx, bx                      ; entry index
mov di, buffer

.search_kernel:
mov si, file_kernel_bin         ; 'KERNEL  BIN'
mov cx, 11                      ; FAT short name = 11 bytes
push di
repe cmpsb                      ; compare 11 bytes
pop di
je .found_kernel

add di, 32
inc bx
cmp bx, [bdb_dir_entries_count]
jl .search_kernel
jmp kernel_not_found_error
```

`repe cmpsb` compares `[es:di]` to `[ds:si]` for up to `cx` bytes,
stopping early on a mismatch. Each directory entry is 32 bytes; the
match target is the literal byte string `KERNEL  BIN` (two spaces
between `KERNEL` and `BIN` because FAT short names pad with spaces).
If we fall off the end without a match, we print `KERNEL.BIN file
not found!` and reboot.

On a match, the first cluster of the file is at offset 26 of the
directory entry (the `FirstClusterLow` field; `FirstClusterHigh` is
zero for FAT12):

```asm
.found_kernel:
mov ax, [di + 26]
mov [kernel_cluster], ax
```

## Reading the FAT

The same `buffer` is reused for the FAT. The root directory is no
longer needed (we already have the cluster number), so the FAT
overwrites it.

```asm
mov ax, [bdb_reserved_sectors]   ; ax = 1
mov bx, buffer
mov cl, [bdb_sectors_per_fat]    ; cl = 9
mov dl, [ebr_drive_number]
call disk_read
```

## Loading the kernel — the FAT chain walk

The kernel is loaded starting at segment `0x2000` (offset 0), which is
physical `0x20000`. Each cluster is one sector, so the loop reads one
sector at a time and walks the FAT to find the next cluster.

```asm
mov bx, KERNEL_LOAD_SEGMENT
mov es, bx
mov bx, KERNEL_LOAD_OFFSET

.load_kernel_loop:
mov ax, [kernel_cluster]
add ax, 31                      ; LBA = cluster + 31
                                ; (1 reserved + 18 fat + 14 root = 33,
                                ;  and cluster 2 is the first data cluster,
                                ;  so the offset is 33 - 2 = 31)
mov cl, 1
mov dl, [ebr_drive_number]
call disk_read

add bx, [bdb_bytes_per_sector]  ; advance the destination pointer

; --- FAT12 chain step ---
mov ax, [kernel_cluster]
mov cx, 3
mul cx                          ; dx:ax = cluster * 3
mov cx, 2
div cx                          ; ax = index into FAT (byte offset /2)
                                ; dx = cluster % 2 (0 = even, 1 = odd)

mov si, buffer
add si, ax
mov ax, [ds:si]                 ; read 16 bits (covers 2 entries)
or dx, dx
jz .even

.odd:
shr ax, 4                       ; high 12 bits
jmp .next_cluster_after

.even:
and ax, 0x0FFF                  ; low 12 bits

.next_cluster_after:
cmp ax, 0x0FF8                  ; end-of-chain marker
jae .read_finish
mov [kernel_cluster], ax
jmp .load_kernel_loop
```

A few details worth calling out:

- The hardcoded `add ax, 31` is the cluster-to-LBA conversion for this
  exact layout (`1 + 18 + 14 = 33`, then `cluster - 2` is the offset
  into the data region). The comment in the file says this is "not
  nice :( hardcoded value" — to make it derived, replace it with
  `sub ax, 2; mov cx, [bdb_sectors_per_cluster]; mul cx; add ax, root_dir_end_lba`.
- FAT12 packs two 12-bit entries into 3 bytes. To read entry
  `cluster`, compute the byte offset `cluster * 3 / 2` and read 2
  bytes (a `uint16`). If `cluster` is even, the entry is in the low
  12 bits; if odd, the high 12 bits. That's exactly what the
  `.even`/`.odd` blocks do.
- The chain ends at any value `>= 0x0FF8`. `0xFFF7` is the bad-cluster
  marker; `0xFFF8..0xFFFF` are end-of-chain. The `jae` covers them
  all.

## Handoff

```asm
.read_finish:
mov dl, [ebr_drive_number]
mov ax, KERNEL_LOAD_SEGMENT
mov ds, ax
mov es, ax
jmp KERNEL_LOAD_SEGMENT:KERNEL_LOAD_OFFSET
```

`ds` and `es` are reloaded to `0x2000` so the kernel sees its data
segment as a normal `ds:bx` window. The far jump lands at
`0x2000:0x0000` (physical `0x20000`), which is exactly the first byte
of the kernel flat binary — that's why `linker.ld` puts
`kernel_entry.o` first inside `.text`.

The `jmp $` after the far jump is a safety net. If the kernel ever
returns, we'd loop here and then fall into the error code.

## Error paths

Two recoverable errors plus one safety halt:

```asm
floppy_error:
    mov si, msg_read_failed
    call puts
    jmp wait_key_and_reboot

kernel_not_found_error:
    mov si, msg_kernel_not_found
    call puts
    jmp wait_key_and_reboot

wait_key_and_reboot:
    mov ah, 0
    int 16h             ; wait for a keypress
    jmp 0FFFFh:0        ; jump to BIOS reset vector — warm reboot
```

`disk_read` also calls `floppy_error` directly if all three retries
are exhausted (see below).

## Subroutines

### `puts` — print a NUL-terminated string

```asm
puts:
    push si
    push ax
    push bx
.loop:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0
    int 0x10
    jmp .loop
.done:
    pop bx
    pop ax
    pop si
    ret
```

`int 0x10 ah=0x0E` is the BIOS teletype: write `al` to the screen at
the current cursor, advance the cursor, wrap at column 80, scroll if
needed. `bh=0` selects display page 0. `lodsb` loads `[ds:si]` into
`al` and increments `si` — so the convention is `ds:si` points at a
NUL-terminated string.

### `lba_to_chs` — convert an LBA to a CHS triple

```asm
lba_to_chs:
    push ax
    push dx

    xor dx, dx
    div word [bdb_sectors_per_track]   ; ax = LBA/SPT, dx = LBA%SPT
    inc dx                             ; sectors are 1-based
    mov cx, dx                         ; cx low 6 bits = sector

    xor dx, dx
    div word [bdb_heads]               ; ax = cylinder, dx = head
    mov dh, dl
    mov ch, al
    shl ah, 6
    or cl, ah                          ; pack top 2 bits of cylinder

    pop ax
    mov dl, al                         ; restore drive number
    pop ax
    ret
```

Standard CHS conversion:

- `sector = (LBA % SPT) + 1` — 1-based
- `cylinder = (LBA / SPT) / heads`
- `head = (LBA / SPT) % heads`
- The cylinder is 10 bits. CHS puts the low 8 bits in `ch` and the
  top 2 bits in the high bits of `cl`. The `shl ah, 6; or cl, ah`
  does that pack.

`al` carries the drive number into the call (we save/restore `dx` and
`ax` and put `al` back into `dl` on the way out so `disk_read` can
read it without an extra argument).

### `disk_read` — BIOS read with retries

```asm
disk_read:
    push ax
    push bx
    push cx
    push dx
    push di

    push cx                  ; remember sector count
    call lba_to_chs
    pop ax                   ; al = sector count to read

    mov ah, 02h
    mov di, 3                ; retry counter

.retry:
    pusha
    stc                      ; some BIOSes require carry=1 going in
    int 13h
    jnc .done                ; carry clear = success

    popa
    call disk_reset
    dec di
    test di, di
    jnz .retry
    jmp floppy_error         ; all retries exhausted

.done:
    popa
    pop di
    pop dx
    pop cx
    pop bx
    pop ax
    ret
```

`int 13h ah=02h` reads `al` sectors starting at CHS `cx/dh` into
`es:bx`. It expects:
- `ah=0x02`
- `al` = number of sectors
- `ch` = cylinder (low 8 bits)
- `cl` = sector number in bits 0..5, cylinder high 2 bits in 6..7
- `dh` = head
- `dl` = drive number
- `es:bx` = destination buffer

The three-retry loop is the standard floppy workaround. `stc` is
defensive — some BIOSes require carry=1 going in, and not setting it
on entry can produce a spurious success.

### `disk_reset` — BIOS reset

```asm
disk_reset:
    pusha
    mov ah, 0
    stc
    int 13h
    jc floppy_error          ; if even reset fails, give up
    popa
    ret
```

`int 13h ah=00h` resets the disk controller for drive `dl`. After a
reset, the next read usually succeeds; that's why the retry loop in
`disk_read` calls reset between attempts.

## What lives at the end of the file

```asm
msg_loading:           db 'Loading...', ENDL, 0
msg_read_failed:       db 'Read from disk failed!', ENDL, 0
msg_kernel_not_found:  db 'KERNEL.BIN file not found!', ENDL, 0
file_kernel_bin:       db 'KERNEL  BIN'
kernel_cluster:        dw 0

KERNEL_LOAD_SEGMENT    equ 0x2000
KERNEL_LOAD_OFFSET     equ 0

times 510-($-$$) db 0
dw 0AA55h

buffer:
```

- `ENDL` is `0x0D, 0x0A` (CR/LF) — teletype output uses both.
- `file_kernel_bin` is the literal 11-byte directory entry name
  (`KERNEL`, two spaces, `BIN`).
- `kernel_cluster` is a 16-bit scratch cell.
- `KERNEL_LOAD_SEGMENT`/`KERNEL_LOAD_OFFSET` are the destination for
  the kernel load.
- The `times 510-($-$$) db 0` fills up to byte 510. `dw 0AA55h`
  places the boot signature at offset 510/511. Without this magic
  the BIOS will refuse to boot the sector.
- `buffer:` is the label for the in-memory sector staging area at
  the end of the boot sector. It overlaps the 0xAA55h signature in
  the sense that it's just past it in the same file — but in memory
  it occupies `0x7E00..0x1FFFF`, which is free.

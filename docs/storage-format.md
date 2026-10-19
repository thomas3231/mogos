# MogOS filesystem format, version 2

MogFS uses 512-byte sectors on the primary ATA disk. Its fixed region is LBA
16–536 inclusive; the disk must contain at least 537 sectors. The editor's
legacy save slot at LBA 1–5 and the intervening sectors are outside the
filesystem and are not modified by MogFS formatting.

| LBA range | Purpose |
| --- | --- |
| 16 | Superblock and active-directory-bank selector |
| 17–20 | Directory bank A |
| 21–24 | Directory bank B |
| 25–536 | Eight file slots, each with two 32-sector data banks |

The directory contains up to 32 entries. Each entry is a file or a directory,
has a parent directory ID, and is protected by CRC-32. Directory updates write
the inactive four-sector bank first and then switch the active bank in the
checksummed superblock. Files can contain up to 16 KiB; rewrites use the
inactive data bank before committing the directory entry. These protocols
preserve the prior committed state when an earlier write reports failure, but
are not a general guarantee against power loss or a device that reports a
failed sector write after committing it.

The format is little-endian and is not a native C structure dump. Names are
1–31 ASCII characters: letters, digits, `.`, `_`, and `-` are accepted except
for `.` and `..`. Directory paths are absolute, slash-separated, and may be
nested. Directories must be empty before deletion.

Formatting is explicit; mounting never formats an unknown, corrupt, or
unsupported volume. Formatting writes only the superblock and both empty
directory banks, leaving file-data sectors and the legacy editor slot intact.
The desktop explorer asks for confirmation before formatting.

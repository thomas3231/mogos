# `heap.c` / `heap.h` — Free-List Allocator

A small first-fit free-list allocator covering an 8 MiB region
of physical memory starting at `0x200000`. There is no
coalescing of arbitrary free pairs — only the immediately
adjacent "next" block is merged on `kfree`.

## Public API (`heap.h`)

```c
void  heap_init(void);
void* kmalloc(unsigned int size);
void  kfree(void* ptr);
```

## Heap layout

```
0x200000                                                    0xA00000
|<------- HEAP_SIZE = 0x800000 (8 MiB) ---------------------->|
[ block ] [ block ] [ block ] [ free/uncarved ... uncarved end ]
                   ^
                   heap_uncarved
```

- `HEAP_START = 0x200000`
- `HEAP_SIZE  = 0x800000` (8 MiB)
- `heap_end   = HEAP_START + HEAP_SIZE = 0xA00000`

The 8 MiB size comfortably covers a 1024×768×32 bpp
backbuffer (3 MiB) with headroom for several more.

## Block header

```c
struct block_header {
    unsigned int       size;   // payload size in bytes (excludes header)
    int                free;   // 0 = in use, 1 = free
    struct block_header* next; // singly-linked list
};
```

Each block is laid out in memory as:

```
[ struct block_header ][ payload of `size` bytes ]
^                       ^
block                   (block + 1) -- the address returned by kmalloc
```

`kmalloc` returns `(void*)(new_block + 1)` — i.e. the address
right after the header. The corresponding `kfree` recomputes
the header by going one header back: `(struct block_header*)ptr - 1`.

## Globals

```c
struct block_header* heap_start_block;  // head of the free list
unsigned char*       heap_uncarved;     // bump pointer for new blocks
unsigned char*       heap_end;          // = HEAP_START + HEAP_SIZE
```

`heap_start_block` is the head of a singly-linked list of all
allocated blocks, in address order. New blocks are appended to
the end. `kfree` doesn't remove blocks from the list — it just
flips `free` to 1 and merges with the next block if the next is
also free.

`heap_uncarved` is the bump pointer. Each `kmalloc` that
doesn't find a reusable free block carves a new block out of the
unallocated region between `heap_uncarved` and `heap_end`.

## `heap_init`

```c
void heap_init(void) {
    heap_start_block = 0;
    heap_uncarved = (unsigned char*)HEAP_START;
    heap_end = (unsigned char*)(HEAP_START + HEAP_SIZE);
}
```

`heap_start_block = 0` means "no blocks yet". The list is
lazily built by the first `kmalloc`. Must be called before any
`kmalloc`/`kfree`.

## `kmalloc`

```c
void* kmalloc(unsigned int size) {
    /* 1. search existing blocks */
    struct block_header* current = heap_start_block;
    struct block_header* prev = 0;
    while (current) {
        if (current->free && current->size >= size) {
            current->free = 0;
            return (void*)(current + 1);
        }
        prev = current;
        current = current->next;
    }

    /* 2. carve a new one from untouched memory */
    unsigned int total_needed = sizeof(struct block_header) + size;
    if (heap_uncarved + total_needed > heap_end) {
        return 0;  // out of memory
    }
    struct block_header* new_block = (struct block_header*)heap_uncarved;
    new_block->size = size;
    new_block->free = 0;
    new_block->next = 0;
    heap_uncarved += total_needed;
    if (prev) {
        prev->next = new_block;
    } else {
        heap_start_block = new_block;
    }
    return (void*)(new_block + 1);
}
```

Two-phase allocation:

1. **Reuse phase.** Walk the block list. The first block with
   `free == 1` and `size >= requested` is reused. The block is
   marked `free = 0` and its payload address is returned.

   No splitting is done — if a 4 KiB free block satisfies a
   64-byte request, the whole 4 KiB is handed back. This is
   wasteful in theory but the kernel makes few allocations, so
   it doesn't matter in practice.

2. **Carve phase.** If no reusable block exists, allocate
   `sizeof(header) + size` bytes from the uncarved region.
   Append the new block to the end of the list and return its
   payload.

If even the carve phase would exceed `heap_end`, return `0`
(OOM). Callers must check for `0`; today only `backbuffer_init`
does, and the heap is large enough that OOM is unlikely with
the current source size.

## `kfree`

```c
void kfree(void* ptr) {
    if (!ptr) return;
    struct block_header* block = (struct block_header*)ptr - 1;
    block->free = 1;

    struct block_header* next = block->next;
    if (next && next->free) {
        block->size += sizeof(struct block_header) + next->size;
        block->next = next->next;
    }
}
```

Marks the block as free, then merges with the immediately
following block (`block->next`) if it's also free. Merging with
the previous block is **not** done — that would require the
block to know its previous neighbor, and the list is
singly-linked. In practice this means that an alternating
alloc/free pattern can fragment the heap, but again, the kernel
allocates very little.

The `if (!ptr) return;` check makes `kfree(NULL)` a safe
no-op, matching the libc convention.

## Quirks and limitations

- **No splitting.** A 4 KiB free block that satisfies a 64-byte
  request is handed back in full. If many tiny allocations
  follow, fragmentation can be severe.
- **No forward-search reuse from a free list of `free` blocks.**
  The list contains *all* blocks (free and in-use), so the
  reuse walk scans every block. For a few dozen blocks this
  is fine; for thousands it would be slow.
- **No size validation on `kfree`.** A bad pointer passed to
  `kfree` corrupts the heap silently.
- **No alignment guarantees.** A `kmalloc(1)` returns a
  1-byte-aligned address. Most callers ask for larger sizes
  (e.g. the backbuffer is `pitch * height`, which is always
  a multiple of 4), so unaligned access isn't an issue today.
- **No thread safety.** No `cli`/`sti` around the list
  mutations. The kernel currently only runs on a single CPU
  and doesn't preempt, so this is safe. Adding preemption
  would require atomic updates.

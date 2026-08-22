#include "heap.h"

#define HEAP_START 0x200000
#define HEAP_SIZE  0x100000

struct block_header {
    unsigned int size;
    int free;
    struct block_header* next;
};

struct block_header* heap_start_block;
unsigned char* heap_uncarved;   // memory not yet split into any block at all
unsigned char* heap_end;

void heap_init(void) {
    heap_start_block = 0;
    heap_uncarved = (unsigned char*)HEAP_START;
    heap_end = (unsigned char*)(HEAP_START + HEAP_SIZE);
}

void* kmalloc(unsigned int size) {
    // 1. Search existing blocks for a free one big enough to reuse
    struct block_header* current = heap_start_block;
    struct block_header* prev = 0;

    while (current) {
        if (current->free && current->size >= size) {
            current->free = 0;
            return (void*)(current + 1);   // data starts right after the header
        }
        prev = current;
        current = current->next;
    }

    // 2. No suitable free block found -- carve a brand new one from untouched memory
    unsigned int total_needed = sizeof(struct block_header) + size;
    if (heap_uncarved + total_needed > heap_end) {
        return 0;   // genuinely out of memory
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

void kfree(void* ptr) {
    if (!ptr) return;

    struct block_header* block = (struct block_header*)ptr - 1;
    block->free = 1;

    // simple coalescing: if the very next block in memory is also free, merge them
    struct block_header* next = block->next;
    if (next && next->free) {
        block->size += sizeof(struct block_header) + next->size;
        block->next = next->next;
    }
}
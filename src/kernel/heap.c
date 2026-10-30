#include "heap.h"
#include <stdint.h>

#define HEAP_START 0x200000
#define HEAP_SIZE  0x800000   // 8MB — comfortably covers even a large VESA framebuffer
#define HEAP_ALIGNMENT 8U

struct block_header {
    unsigned int size;
    int free;
    struct block_header* next;
};

static struct block_header* heap_start_block;
static uintptr_t heap_uncarved;
static uintptr_t heap_end;

void heap_init(void) {
    heap_init_region((void*)HEAP_START, HEAP_SIZE);
}

int heap_init_region(void* memory, unsigned int size) {
    uintptr_t start = (uintptr_t)memory;
    uintptr_t end;
    uintptr_t aligned_start;

    heap_start_block = 0;
    heap_uncarved = 0;
    heap_end = 0;
    if (!memory || start > UINTPTR_MAX - size ||
        start > UINTPTR_MAX - (HEAP_ALIGNMENT - 1U)) return 0;

    end = start + size;
    aligned_start = (start + HEAP_ALIGNMENT - 1U) &
                    ~((uintptr_t)HEAP_ALIGNMENT - 1U);
    if (aligned_start > end ||
        end - aligned_start < sizeof(struct block_header) +
                             HEAP_ALIGNMENT) {
        return 0;
    }

    heap_uncarved = aligned_start;
    heap_end = end;
    return 1;
}

static unsigned int aligned_header_size(void) {
    return (sizeof(struct block_header) + HEAP_ALIGNMENT - 1U) &
           ~(HEAP_ALIGNMENT - 1U);
}

void* kmalloc(unsigned int size) {
    unsigned int header_size = aligned_header_size();
    unsigned int aligned_size;
    struct block_header* current = heap_start_block;
    struct block_header* prev = 0;

    if (!size || size > ~0U - (HEAP_ALIGNMENT - 1U)) return 0;
    aligned_size = (size + HEAP_ALIGNMENT - 1U) &
                   ~(HEAP_ALIGNMENT - 1U);

    while (current) {
        if (current->free && current->size >= aligned_size) {
            current->free = 0;
            return (void*)((uintptr_t)current + header_size);
        }
        prev = current;
        current = current->next;
    }

    if (heap_uncarved > heap_end ||
        aligned_size > heap_end - heap_uncarved ||
        header_size > heap_end - heap_uncarved - aligned_size) return 0;

    struct block_header* new_block = (struct block_header*)heap_uncarved;
    new_block->size = aligned_size;
    new_block->free = 0;
    new_block->next = 0;

    heap_uncarved += header_size + aligned_size;

    if (prev) {
        prev->next = new_block;
    } else {
        heap_start_block = new_block;
    }

    return (void*)((uintptr_t)new_block + header_size);
}

void kfree(void* ptr) {
    if (!ptr) return;

    struct block_header* current = heap_start_block;
    uintptr_t payload = (uintptr_t)ptr;
    unsigned int header_size = aligned_header_size();
    int found = 0;

    while (current) {
        if ((uintptr_t)current + header_size == payload) {
            if (current->free) return;
            current->free = 1;
            found = 1;
            break;
        }
        current = current->next;
    }
    if (!found) return;

    current = heap_start_block;
    while (current && current->next) {
        if (current->free && current->next->free) {
            current->size += header_size + current->next->size;
            current->next = current->next->next;
        } else {
            current = current->next;
        }
    }
}
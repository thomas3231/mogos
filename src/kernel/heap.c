#include "heap.h"

#define HEAP_START 0x200000   // 2MB mark — well past your kernel, bootloader, and stack
#define HEAP_SIZE  0x100000   // 1MB heap, plenty for icons/windows for a long while

unsigned char* heap_next;
unsigned char* heap_end;

void heap_init(void) {
    heap_next = (unsigned char*)HEAP_START;
    heap_end  = (unsigned char*)(HEAP_START + HEAP_SIZE);
}

void* kmalloc(unsigned int size) {
    if (heap_next + size > heap_end) {
        return 0;   // out of memory — you'll want to handle this eventually, not today
    }
    void* result = heap_next;
    heap_next += size;
    return result;
}
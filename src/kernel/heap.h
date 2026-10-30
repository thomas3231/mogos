#ifndef HEAP_H
#define HEAP_H

void heap_init(void);
int heap_init_region(void* memory, unsigned int size);
void* kmalloc(unsigned int size);
void kfree(void* ptr);

#endif
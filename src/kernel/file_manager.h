#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <stdint.h>

#define FILE_MANAGER_BROWSE 0
#define FILE_MANAGER_OPEN 1
#define FILE_MANAGER_SAVE 2

void file_manager_start(int mode);
void file_manager_handle_key(char key);
int file_manager_read_file(const char* path, unsigned char* buffer,
                           uint32_t capacity, uint32_t* size);
int file_manager_write_file(const char* path, const unsigned char* buffer,
                            uint32_t size);

#endif

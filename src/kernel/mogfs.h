#ifndef MOGFS_H
#define MOGFS_H

#include <stdint.h>

#define MOGFS_SECTOR_SIZE 512U
#define MOGFS_START_LBA 16U
#define MOGFS_FILE_COUNT 8U
#define MOGFS_ENTRY_COUNT 32U
#define MOGFS_DIRECTORY_SECTORS 4U
#define MOGFS_DIRECTORY_BANK_COUNT 2U
#define MOGFS_FILE_NAME_SIZE 32U
#define MOGFS_PATH_SIZE 128U
#define MOGFS_FILE_MAX_SIZE (16U * 1024U)
#define MOGFS_REQUIRED_SECTORS \
    (1U + MOGFS_DIRECTORY_BANK_COUNT * MOGFS_DIRECTORY_SECTORS + \
     MOGFS_FILE_COUNT * 64U)

typedef int (*mogfs_read_sector_fn)(void* context, uint32_t lba,
                                    unsigned char* buffer);
typedef int (*mogfs_write_sector_fn)(void* context, uint32_t lba,
                                     unsigned char* buffer);

struct mogfs_block_device {
    void* context;
    uint32_t sector_count;
    mogfs_read_sector_fn read_sector;
    mogfs_write_sector_fn write_sector;
};

struct mogfs {
    struct mogfs_block_device device;
    int mounted;
    unsigned int active_directory_bank;
};

struct mogfs_file_info {
    char name[MOGFS_FILE_NAME_SIZE];
    uint32_t size;
    int is_directory;
};

enum mogfs_result {
    MOGFS_OK = 0,
    MOGFS_ERROR_INVALID_ARGUMENT,
    MOGFS_ERROR_NOT_MOUNTED,
    MOGFS_ERROR_IO,
    MOGFS_ERROR_CORRUPT,
    MOGFS_ERROR_UNSUPPORTED,
    MOGFS_ERROR_NO_SPACE,
    MOGFS_ERROR_NOT_FOUND,
    MOGFS_ERROR_ALREADY_EXISTS,
    MOGFS_ERROR_TOO_LARGE,
    MOGFS_ERROR_BUFFER_TOO_SMALL,
    MOGFS_ERROR_NOT_EMPTY
};

int mogfs_format(const struct mogfs_block_device* device);
int mogfs_ata_device_init(struct mogfs_block_device* device);
int mogfs_mount(struct mogfs* filesystem,
                const struct mogfs_block_device* device);
int mogfs_list(struct mogfs* filesystem, const char* directory_path,
               struct mogfs_file_info* entries, uint32_t capacity,
               uint32_t* count);
int mogfs_create_file(struct mogfs* filesystem, const char* directory_path,
                      const char* name);
int mogfs_create_directory(struct mogfs* filesystem, const char* directory_path,
                           const char* name);
int mogfs_read(struct mogfs* filesystem, const char* path,
               unsigned char* buffer, uint32_t capacity, uint32_t* size);
int mogfs_write(struct mogfs* filesystem, const char* path,
                const unsigned char* buffer, uint32_t size);
int mogfs_delete(struct mogfs* filesystem, const char* path);

#endif

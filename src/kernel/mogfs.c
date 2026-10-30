#include "mogfs.h"
#include "ata.h"

#define MOGFS_VERSION 2U
#define MOGFS_SUPER_LBA MOGFS_START_LBA
#define MOGFS_DIRECTORY_LBA (MOGFS_START_LBA + 1U)
#define MOGFS_DATA_LBA \
    (MOGFS_DIRECTORY_LBA + MOGFS_DIRECTORY_BANK_COUNT * MOGFS_DIRECTORY_SECTORS)
#define MOGFS_ENTRY_SIZE 64U
#define MOGFS_ENTRIES_PER_SECTOR (MOGFS_SECTOR_SIZE / MOGFS_ENTRY_SIZE)
#define MOGFS_BANK_SECTORS 32U
#define MOGFS_SLOT_SECTORS (MOGFS_BANK_SECTORS * 2U)
#define MOGFS_ENTRY_STATE_OFFSET 36U
#define MOGFS_ENTRY_TYPE_OFFSET 37U
#define MOGFS_ENTRY_BANK_OFFSET 38U
#define MOGFS_ENTRY_SLOT_OFFSET 39U
#define MOGFS_ENTRY_PARENT_OFFSET 40U
#define MOGFS_ENTRY_CRC_OFFSET 60U
#define MOGFS_SUPER_CRC_OFFSET 508U
#define MOGFS_TYPE_FILE 1U
#define MOGFS_TYPE_DIRECTORY 2U

static const unsigned char mogfs_magic[8] = {
    'M', 'O', 'G', 'F', 'S', '0', '0', '1'
};

static uint32_t read_u32(const unsigned char* buffer) {
    return (uint32_t)buffer[0] |
           ((uint32_t)buffer[1] << 8) |
           ((uint32_t)buffer[2] << 16) |
           ((uint32_t)buffer[3] << 24);
}

static uint16_t read_u16(const unsigned char* buffer) {
    return (uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8);
}

static void write_u32(unsigned char* buffer, uint32_t value) {
    buffer[0] = (unsigned char)value;
    buffer[1] = (unsigned char)(value >> 8);
    buffer[2] = (unsigned char)(value >> 16);
    buffer[3] = (unsigned char)(value >> 24);
}

static void write_u16(unsigned char* buffer, uint16_t value) {
    buffer[0] = (unsigned char)value;
    buffer[1] = (unsigned char)(value >> 8);
}

static uint32_t crc32(const unsigned char* data, uint32_t size) {
    uint32_t crc = 0xFFFFFFFFU;
    for (uint32_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (uint32_t bit = 0; bit < 8; bit++) {
            uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1) ^ (0xEDB88320U & mask);
        }
    }
    return ~crc;
}

static int device_is_valid(const struct mogfs_block_device* device) {
    return device && device->read_sector && device->write_sector &&
           device->sector_count >= MOGFS_START_LBA + MOGFS_REQUIRED_SECTORS;
}

static int ata_block_read(void* context, uint32_t lba,
                          unsigned char* buffer) {
    (void)context;
    return ata_read_sector(lba, buffer);
}

static int ata_block_write(void* context, uint32_t lba,
                           unsigned char* buffer) {
    (void)context;
    return ata_write_sector(lba, buffer);
}

int mogfs_ata_device_init(struct mogfs_block_device* device) {
    unsigned int sector_count;
    if (!device || !ata_get_sector_count(&sector_count)) return 0;
    device->context = 0;
    device->sector_count = sector_count;
    device->read_sector = ata_block_read;
    device->write_sector = ata_block_write;
    return 1;
}

static int valid_component(const char* name, uint32_t* length) {
    uint32_t i = 0;
    if (!name) return 0;
    while (i < MOGFS_FILE_NAME_SIZE && name[i]) {
        unsigned char c = (unsigned char)name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) {
            return 0;
        }
        i++;
    }
    if (i == 0 || i >= MOGFS_FILE_NAME_SIZE ||
        (i == 1 && name[0] == '.') ||
        (i == 2 && name[0] == '.' && name[1] == '.')) {
        return 0;
    }
    *length = i;
    return 1;
}

static int valid_path(const char* path, int allow_root) {
    uint32_t i = 0;
    if (!path || path[0] != '/') return 0;
    if (path[1] == '\0') return allow_root;
    while (path[i]) {
        uint32_t start;
        while (path[i] == '/') i++;
        if (!path[i]) break;
        start = i;
        while (path[i] && path[i] != '/') i++;
        if (i - start >= MOGFS_FILE_NAME_SIZE) return 0;
        if ((i - start == 1 && path[start] == '.') ||
            (i - start == 2 && path[start] == '.' && path[start + 1] == '.')) {
            return 0;
        }
        for (uint32_t j = start; j < i; j++) {
            unsigned char c = (unsigned char)path[j];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) {
                return 0;
            }
        }
    }
    return i < MOGFS_PATH_SIZE;
}

static unsigned char* entry_at(unsigned char* directory, uint32_t index) {
    return directory + index * MOGFS_ENTRY_SIZE;
}

static const unsigned char* const_entry_at(const unsigned char* directory,
                                           uint32_t index) {
    return directory + index * MOGFS_ENTRY_SIZE;
}

static int entry_is_zero(const unsigned char* entry) {
    for (uint32_t i = 0; i < MOGFS_ENTRY_SIZE; i++) {
        if (entry[i]) return 0;
    }
    return 1;
}

static uint32_t entry_crc(const unsigned char* entry) {
    return crc32(entry, MOGFS_ENTRY_CRC_OFFSET);
}

static void set_entry_crc(unsigned char* entry) {
    write_u32(entry + MOGFS_ENTRY_CRC_OFFSET, entry_crc(entry));
}

static int entry_is_active(const unsigned char* entry) {
    return entry[MOGFS_ENTRY_STATE_OFFSET] == 1;
}

static int validate_entry(const unsigned char* entry) {
    uint32_t length = 0;
    uint32_t size = read_u32(entry + 32);
    uint32_t type = entry[MOGFS_ENTRY_TYPE_OFFSET];

    if (entry[MOGFS_ENTRY_STATE_OFFSET] == 0) return entry_is_zero(entry);
    if (entry[MOGFS_ENTRY_STATE_OFFSET] != 1 ||
        (type != MOGFS_TYPE_FILE && type != MOGFS_TYPE_DIRECTORY) ||
        read_u32(entry + MOGFS_ENTRY_CRC_OFFSET) != entry_crc(entry) ||
        !valid_component((const char*)entry, &length) ||
        entry[length] != '\0') {
        return 0;
    }
    for (uint32_t i = length + 1; i < 32; i++) {
        if (entry[i]) return 0;
    }
    for (uint32_t i = 42; i < MOGFS_ENTRY_CRC_OFFSET; i++) {
        if (entry[i]) return 0;
    }
    if (type == MOGFS_TYPE_DIRECTORY) {
        return size == 0 && entry[MOGFS_ENTRY_BANK_OFFSET] == 0 &&
               entry[MOGFS_ENTRY_SLOT_OFFSET] == 0;
    }
    return size <= MOGFS_FILE_MAX_SIZE &&
           entry[MOGFS_ENTRY_BANK_OFFSET] < 2 &&
           entry[MOGFS_ENTRY_SLOT_OFFSET] < MOGFS_FILE_COUNT;
}

static int names_equal(const unsigned char* entry, const char* name) {
    uint32_t i = 0;
    while (entry[i] && name[i] && entry[i] == (unsigned char)name[i]) i++;
    return !entry[i] && !name[i];
}

static int validate_directory(const unsigned char* directory) {
    unsigned char visited[MOGFS_ENTRY_COUNT];

    for (uint32_t i = 0; i < MOGFS_ENTRY_COUNT; i++) {
        const unsigned char* entry = const_entry_at(directory, i);
        if (!validate_entry(entry)) return 0;
        if (!entry_is_active(entry)) continue;

        uint16_t parent = read_u16(entry + MOGFS_ENTRY_PARENT_OFFSET);
        if (parent > MOGFS_ENTRY_COUNT) return 0;
        if (parent != 0) {
            const unsigned char* parent_entry =
                const_entry_at(directory, parent - 1U);
            if (!entry_is_active(parent_entry) ||
                parent_entry[MOGFS_ENTRY_TYPE_OFFSET] != MOGFS_TYPE_DIRECTORY) {
                return 0;
            }
        }
        for (uint32_t j = 0; j < i; j++) {
            const unsigned char* previous = const_entry_at(directory, j);
            if (entry_is_active(previous) &&
                read_u16(previous + MOGFS_ENTRY_PARENT_OFFSET) == parent &&
                names_equal(previous, (const char*)entry)) {
                return 0;
            }
        }

        for (uint32_t j = 0; j < MOGFS_ENTRY_COUNT; j++) visited[j] = 0;
        uint16_t ancestor = parent;
        for (uint32_t depth = 0; ancestor != 0; depth++) {
            if (ancestor > MOGFS_ENTRY_COUNT ||
                depth >= MOGFS_ENTRY_COUNT || visited[ancestor - 1U]) {
                return 0;
            }
            visited[ancestor - 1U] = 1;
            ancestor = read_u16(
                const_entry_at(directory, ancestor - 1U) +
                MOGFS_ENTRY_PARENT_OFFSET);
        }
    }

    unsigned char slots[MOGFS_FILE_COUNT] = {0};
    for (uint32_t i = 0; i < MOGFS_ENTRY_COUNT; i++) {
        const unsigned char* entry = const_entry_at(directory, i);
        if (entry_is_active(entry) &&
            entry[MOGFS_ENTRY_TYPE_OFFSET] == MOGFS_TYPE_FILE) {
            uint32_t slot = entry[MOGFS_ENTRY_SLOT_OFFSET];
            if (slots[slot]) return 0;
            slots[slot] = 1;
        }
    }
    return 1;
}

static int read_directory(struct mogfs* filesystem,
                          unsigned char* directory) {
    if (!filesystem || !filesystem->mounted) return MOGFS_ERROR_NOT_MOUNTED;
    for (uint32_t i = 0; i < MOGFS_DIRECTORY_SECTORS; i++) {
        if (!filesystem->device.read_sector(
                filesystem->device.context,
                MOGFS_DIRECTORY_LBA +
                    filesystem->active_directory_bank * MOGFS_DIRECTORY_SECTORS +
                    i,
                directory + i * MOGFS_SECTOR_SIZE)) {
            return MOGFS_ERROR_IO;
        }
    }
    return validate_directory(directory) ? MOGFS_OK : MOGFS_ERROR_CORRUPT;
}

static int write_directory(struct mogfs* filesystem,
                           unsigned char* directory) {
    unsigned char superblock[MOGFS_SECTOR_SIZE];
    unsigned int new_bank = 1U - filesystem->active_directory_bank;
    for (uint32_t i = 0; i < MOGFS_DIRECTORY_SECTORS; i++) {
        if (!filesystem->device.write_sector(
                filesystem->device.context,
                MOGFS_DIRECTORY_LBA + new_bank * MOGFS_DIRECTORY_SECTORS + i,
                directory + i * MOGFS_SECTOR_SIZE)) {
            return MOGFS_ERROR_IO;
        }
    }
    for (uint32_t i = 0; i < MOGFS_SECTOR_SIZE; i++) superblock[i] = 0;
    for (uint32_t i = 0; i < sizeof(mogfs_magic); i++) {
        superblock[i] = mogfs_magic[i];
    }
    write_u32(superblock + 8, MOGFS_VERSION);
    write_u32(superblock + 12, MOGFS_REQUIRED_SECTORS);
    write_u32(superblock + 16, MOGFS_DIRECTORY_LBA);
    write_u32(superblock + 20, MOGFS_ENTRY_COUNT);
    write_u32(superblock + 24, MOGFS_FILE_COUNT);
    write_u32(superblock + 28, MOGFS_FILE_MAX_SIZE);
    write_u32(superblock + 32, MOGFS_START_LBA);
    write_u32(superblock + 36, new_bank);
    write_u32(superblock + MOGFS_SUPER_CRC_OFFSET,
              crc32(superblock, MOGFS_SUPER_CRC_OFFSET));
    if (!filesystem->device.write_sector(filesystem->device.context,
                                          MOGFS_SUPER_LBA, superblock)) {
        return MOGFS_ERROR_IO;
    }
    filesystem->active_directory_bank = new_bank;
    return MOGFS_OK;
}

static int validate_superblock(const unsigned char* superblock) {
    if (read_u32(superblock + MOGFS_SUPER_CRC_OFFSET) !=
        crc32(superblock, MOGFS_SUPER_CRC_OFFSET)) {
        return MOGFS_ERROR_CORRUPT;
    }
    for (uint32_t i = 0; i < 8; i++) {
        if (superblock[i] != mogfs_magic[i]) return MOGFS_ERROR_CORRUPT;
    }
    if (read_u32(superblock + 8) != MOGFS_VERSION) {
        return MOGFS_ERROR_UNSUPPORTED;
    }
    if (read_u32(superblock + 12) != MOGFS_REQUIRED_SECTORS ||
        read_u32(superblock + 16) != MOGFS_DIRECTORY_LBA ||
        read_u32(superblock + 20) != MOGFS_ENTRY_COUNT ||
        read_u32(superblock + 24) != MOGFS_FILE_COUNT ||
        read_u32(superblock + 28) != MOGFS_FILE_MAX_SIZE ||
        read_u32(superblock + 32) != MOGFS_START_LBA ||
        read_u32(superblock + 36) >= MOGFS_DIRECTORY_BANK_COUNT) {
        return MOGFS_ERROR_CORRUPT;
    }
    for (uint32_t i = 40; i < MOGFS_SUPER_CRC_OFFSET; i++) {
        if (superblock[i]) return MOGFS_ERROR_CORRUPT;
    }
    return MOGFS_OK;
}

int mogfs_format(const struct mogfs_block_device* device) {
    unsigned char directory[MOGFS_DIRECTORY_SECTORS * MOGFS_SECTOR_SIZE] = {0};
    unsigned char superblock[MOGFS_SECTOR_SIZE] = {0};

    if (!device_is_valid(device)) return MOGFS_ERROR_INVALID_ARGUMENT;
    for (uint32_t bank = 0; bank < MOGFS_DIRECTORY_BANK_COUNT; bank++) {
        for (uint32_t i = 0; i < MOGFS_DIRECTORY_SECTORS; i++) {
            if (!device->write_sector(
                    device->context,
                    MOGFS_DIRECTORY_LBA +
                        bank * MOGFS_DIRECTORY_SECTORS + i,
                    directory + i * MOGFS_SECTOR_SIZE)) {
                return MOGFS_ERROR_IO;
            }
        }
    }

    for (uint32_t i = 0; i < sizeof(mogfs_magic); i++) {
        superblock[i] = mogfs_magic[i];
    }
    write_u32(superblock + 8, MOGFS_VERSION);
    write_u32(superblock + 12, MOGFS_REQUIRED_SECTORS);
    write_u32(superblock + 16, MOGFS_DIRECTORY_LBA);
    write_u32(superblock + 20, MOGFS_ENTRY_COUNT);
    write_u32(superblock + 24, MOGFS_FILE_COUNT);
    write_u32(superblock + 28, MOGFS_FILE_MAX_SIZE);
    write_u32(superblock + 32, MOGFS_START_LBA);
    write_u32(superblock + 36, 0);
    write_u32(superblock + MOGFS_SUPER_CRC_OFFSET,
              crc32(superblock, MOGFS_SUPER_CRC_OFFSET));
    if (!device->write_sector(device->context, MOGFS_SUPER_LBA, superblock)) {
        return MOGFS_ERROR_IO;
    }
    return MOGFS_OK;
}

int mogfs_mount(struct mogfs* filesystem,
                const struct mogfs_block_device* device) {
    unsigned char superblock[MOGFS_SECTOR_SIZE];
    unsigned char directory[MOGFS_DIRECTORY_SECTORS * MOGFS_SECTOR_SIZE];
    unsigned int active_bank;
    int result;

    if (!filesystem) return MOGFS_ERROR_INVALID_ARGUMENT;
    filesystem->mounted = 0;
    if (!device_is_valid(device)) return MOGFS_ERROR_INVALID_ARGUMENT;
    if (!device->read_sector(device->context, MOGFS_SUPER_LBA, superblock)) {
        return MOGFS_ERROR_IO;
    }
    result = validate_superblock(superblock);
    if (result != MOGFS_OK) return result;
    active_bank = read_u32(superblock + 36);
    for (uint32_t i = 0; i < MOGFS_DIRECTORY_SECTORS; i++) {
        if (!device->read_sector(
                device->context,
                MOGFS_DIRECTORY_LBA + active_bank * MOGFS_DIRECTORY_SECTORS + i,
                                 directory + i * MOGFS_SECTOR_SIZE)) {
            return MOGFS_ERROR_IO;
        }
    }
    if (!validate_directory(directory)) return MOGFS_ERROR_CORRUPT;

    filesystem->device = *device;
    filesystem->mounted = 1;
    filesystem->active_directory_bank = active_bank;
    return MOGFS_OK;
}

static int find_entry_id(const unsigned char* directory, uint16_t parent,
                         const char* name) {
    for (uint32_t i = 0; i < MOGFS_ENTRY_COUNT; i++) {
        const unsigned char* entry = const_entry_at(directory, i);
        if (entry_is_active(entry) &&
            read_u16(entry + MOGFS_ENTRY_PARENT_OFFSET) == parent &&
            names_equal(entry, name)) {
            return (int)i + 1;
        }
    }
    return -1;
}

static int resolve_directory(const unsigned char* directory,
                             const char* path, uint16_t* id) {
    uint32_t position = 1;
    uint16_t current = 0;

    if (!valid_path(path, 1)) return MOGFS_ERROR_INVALID_ARGUMENT;
    while (path[position]) {
        char component[MOGFS_FILE_NAME_SIZE];
        uint32_t length = 0;
        while (path[position] == '/') position++;
        if (!path[position]) break;
        while (path[position] && path[position] != '/') {
            if (length >= MOGFS_FILE_NAME_SIZE - 1) {
                return MOGFS_ERROR_INVALID_ARGUMENT;
            }
            component[length++] = path[position++];
        }
        component[length] = '\0';
        int found = find_entry_id(directory, current, component);
        if (found < 0) return MOGFS_ERROR_NOT_FOUND;
        const unsigned char* entry = const_entry_at(directory, found - 1);
        if (entry[MOGFS_ENTRY_TYPE_OFFSET] != MOGFS_TYPE_DIRECTORY) {
            return MOGFS_ERROR_NOT_FOUND;
        }
        current = (uint16_t)found;
    }
    *id = current;
    return MOGFS_OK;
}

static int resolve_path(const unsigned char* directory, const char* path,
                        uint16_t* parent, char* basename) {
    uint32_t length = 0;
    uint32_t end = 0;
    uint32_t start;
    uint32_t parent_end;
    char parent_path[MOGFS_PATH_SIZE];
    int result;

    if (!valid_path(path, 0)) return MOGFS_ERROR_INVALID_ARGUMENT;
    while (path[end]) end++;
    start = end;
    while (start > 0 && path[start - 1] != '/') start--;
    length = end - start;
    for (uint32_t i = 0; i < length; i++) basename[i] = path[start + i];
    basename[length] = '\0';

    parent_end = start;
    if (parent_end == 1) {
        parent_path[0] = '/';
        parent_path[1] = '\0';
    } else {
        if (parent_end == 0 || parent_end >= MOGFS_PATH_SIZE) {
            return MOGFS_ERROR_INVALID_ARGUMENT;
        }
        parent_end--;
        for (uint32_t i = 0; i < parent_end; i++) parent_path[i] = path[i];
        parent_path[parent_end] = '\0';
    }

    result = resolve_directory(directory, parent_path, parent);
    return result;
}

static uint32_t file_bank_lba(uint32_t slot, uint32_t bank) {
    return MOGFS_DATA_LBA + slot * MOGFS_SLOT_SECTORS +
           bank * MOGFS_BANK_SECTORS;
}

int mogfs_list(struct mogfs* filesystem, const char* directory_path,
               struct mogfs_file_info* entries, uint32_t capacity,
               uint32_t* count) {
    unsigned char directory[MOGFS_DIRECTORY_SECTORS * MOGFS_SECTOR_SIZE];
    uint16_t parent;
    uint32_t entry_count = 0;
    int result;

    if (!entries || !count) return MOGFS_ERROR_INVALID_ARGUMENT;
    result = read_directory(filesystem, directory);
    if (result != MOGFS_OK) return result;
    result = resolve_directory(directory, directory_path, &parent);
    if (result != MOGFS_OK) return result;

    for (uint32_t i = 0; i < MOGFS_ENTRY_COUNT; i++) {
        const unsigned char* entry = const_entry_at(directory, i);
        if (entry_is_active(entry) &&
            read_u16(entry + MOGFS_ENTRY_PARENT_OFFSET) == parent) {
            entry_count++;
        }
    }
    if (capacity < entry_count) return MOGFS_ERROR_BUFFER_TOO_SMALL;

    entry_count = 0;
    for (uint32_t i = 0; i < MOGFS_ENTRY_COUNT; i++) {
        const unsigned char* entry = const_entry_at(directory, i);
        if (entry_is_active(entry) &&
            read_u16(entry + MOGFS_ENTRY_PARENT_OFFSET) == parent) {
            uint32_t j = 0;
            while (entry[j]) {
                entries[entry_count].name[j] = (char)entry[j];
                j++;
            }
            entries[entry_count].name[j] = '\0';
            entries[entry_count].size = read_u32(entry + 32);
            entries[entry_count].is_directory =
                entry[MOGFS_ENTRY_TYPE_OFFSET] == MOGFS_TYPE_DIRECTORY;
            entry_count++;
        }
    }
    *count = entry_count;
    return MOGFS_OK;
}

static int create_entry(struct mogfs* filesystem, const char* directory_path,
                        const char* name, uint32_t type) {
    unsigned char directory[MOGFS_DIRECTORY_SECTORS * MOGFS_SECTOR_SIZE];
    uint32_t length;
    uint16_t parent;
    int free_entry = -1;
    int free_file_slot = -1;
    int result;

    if (!valid_component(name, &length)) return MOGFS_ERROR_INVALID_ARGUMENT;
    result = read_directory(filesystem, directory);
    if (result != MOGFS_OK) return result;
    result = resolve_directory(directory, directory_path, &parent);
    if (result != MOGFS_OK) return result;
    if (find_entry_id(directory, parent, name) >= 0) {
        return MOGFS_ERROR_ALREADY_EXISTS;
    }
    for (uint32_t i = 0; i < MOGFS_ENTRY_COUNT; i++) {
        unsigned char* entry = entry_at(directory, i);
        if (!entry_is_active(entry) && free_entry < 0) free_entry = (int)i;
    }
    if (free_entry < 0) return MOGFS_ERROR_NO_SPACE;
    if (type == MOGFS_TYPE_FILE) {
        for (uint32_t candidate = 0; candidate < MOGFS_FILE_COUNT; candidate++) {
            int used = 0;
            for (uint32_t i = 0; i < MOGFS_ENTRY_COUNT; i++) {
                const unsigned char* entry = const_entry_at(directory, i);
                if (entry_is_active(entry) &&
                    entry[MOGFS_ENTRY_TYPE_OFFSET] == MOGFS_TYPE_FILE &&
                    entry[MOGFS_ENTRY_SLOT_OFFSET] == candidate) {
                    used = 1;
                    break;
                }
            }
            if (!used) {
                free_file_slot = (int)candidate;
                break;
            }
        }
        if (free_file_slot < 0) return MOGFS_ERROR_NO_SPACE;
    }

    unsigned char* entry = entry_at(directory, (uint32_t)free_entry);
    for (uint32_t i = 0; i < length; i++) entry[i] = (unsigned char)name[i];
    entry[MOGFS_ENTRY_STATE_OFFSET] = 1;
    entry[MOGFS_ENTRY_TYPE_OFFSET] = (unsigned char)type;
    entry[MOGFS_ENTRY_BANK_OFFSET] = 0;
    entry[MOGFS_ENTRY_SLOT_OFFSET] =
        type == MOGFS_TYPE_FILE ? (unsigned char)free_file_slot : 0;
    write_u16(entry + MOGFS_ENTRY_PARENT_OFFSET, parent);
    set_entry_crc(entry);
    return write_directory(filesystem, directory);
}

int mogfs_create_file(struct mogfs* filesystem, const char* directory_path,
                      const char* name) {
    return create_entry(filesystem, directory_path, name, MOGFS_TYPE_FILE);
}

int mogfs_create_directory(struct mogfs* filesystem, const char* directory_path,
                           const char* name) {
    return create_entry(filesystem, directory_path, name, MOGFS_TYPE_DIRECTORY);
}

static int find_file_path(struct mogfs* filesystem, const char* path,
                          unsigned char* directory, unsigned char** entry) {
    char basename[MOGFS_FILE_NAME_SIZE];
    uint16_t parent;
    int result = read_directory(filesystem, directory);
    if (result != MOGFS_OK) return result;
    result = resolve_path(directory, path, &parent, basename);
    if (result != MOGFS_OK) return result;
    int id = find_entry_id(directory, parent, basename);
    if (id < 0) return MOGFS_ERROR_NOT_FOUND;
    *entry = entry_at(directory, (uint32_t)id - 1U);
    if ((*entry)[MOGFS_ENTRY_TYPE_OFFSET] != MOGFS_TYPE_FILE) {
        return MOGFS_ERROR_INVALID_ARGUMENT;
    }
    return MOGFS_OK;
}

int mogfs_read(struct mogfs* filesystem, const char* path,
               unsigned char* buffer, uint32_t capacity, uint32_t* size) {
    unsigned char directory[MOGFS_DIRECTORY_SECTORS * MOGFS_SECTOR_SIZE];
    unsigned char sector[MOGFS_SECTOR_SIZE];
    unsigned char* entry;
    int result;

    if (!size) return MOGFS_ERROR_INVALID_ARGUMENT;
    result = find_file_path(filesystem, path, directory, &entry);
    if (result != MOGFS_OK) return result;
    uint32_t file_size = read_u32(entry + 32);
    if (capacity < file_size) return MOGFS_ERROR_BUFFER_TOO_SMALL;
    if (file_size && !buffer) return MOGFS_ERROR_INVALID_ARGUMENT;

    uint32_t bank_lba = file_bank_lba(entry[MOGFS_ENTRY_SLOT_OFFSET],
                                      entry[MOGFS_ENTRY_BANK_OFFSET]);
    uint32_t offset = 0;
    while (offset < file_size) {
        if (!filesystem->device.read_sector(
                filesystem->device.context,
                bank_lba + offset / MOGFS_SECTOR_SIZE, sector)) {
            return MOGFS_ERROR_IO;
        }
        uint32_t amount = file_size - offset;
        if (amount > MOGFS_SECTOR_SIZE) amount = MOGFS_SECTOR_SIZE;
        for (uint32_t i = 0; i < amount; i++) buffer[offset + i] = sector[i];
        offset += amount;
    }
    *size = file_size;
    return MOGFS_OK;
}

int mogfs_write(struct mogfs* filesystem, const char* path,
                const unsigned char* buffer, uint32_t size) {
    unsigned char directory[MOGFS_DIRECTORY_SECTORS * MOGFS_SECTOR_SIZE];
    unsigned char sector[MOGFS_SECTOR_SIZE];
    unsigned char* entry;
    int result;

    if (size && !buffer) return MOGFS_ERROR_INVALID_ARGUMENT;
    if (size > MOGFS_FILE_MAX_SIZE) return MOGFS_ERROR_TOO_LARGE;
    result = find_file_path(filesystem, path, directory, &entry);
    if (result != MOGFS_OK) return result;

    uint32_t new_bank = 1U - entry[MOGFS_ENTRY_BANK_OFFSET];
    uint32_t bank_lba = file_bank_lba(entry[MOGFS_ENTRY_SLOT_OFFSET], new_bank);
    uint32_t offset = 0;
    while (offset < size) {
        for (uint32_t i = 0; i < MOGFS_SECTOR_SIZE; i++) sector[i] = 0;
        uint32_t amount = size - offset;
        if (amount > MOGFS_SECTOR_SIZE) amount = MOGFS_SECTOR_SIZE;
        for (uint32_t i = 0; i < amount; i++) sector[i] = buffer[offset + i];
        if (!filesystem->device.write_sector(
                filesystem->device.context,
                bank_lba + offset / MOGFS_SECTOR_SIZE, sector)) {
            return MOGFS_ERROR_IO;
        }
        offset += amount;
    }

    write_u32(entry + 32, size);
    entry[MOGFS_ENTRY_BANK_OFFSET] = (unsigned char)new_bank;
    set_entry_crc(entry);
    return write_directory(filesystem, directory);
}

int mogfs_delete(struct mogfs* filesystem, const char* path) {
    unsigned char directory[MOGFS_DIRECTORY_SECTORS * MOGFS_SECTOR_SIZE];
    unsigned char remove_entry[MOGFS_ENTRY_COUNT] = {0};
    char basename[MOGFS_FILE_NAME_SIZE];
    uint16_t parent;
    int result = read_directory(filesystem, directory);
    if (result != MOGFS_OK) return result;
    result = resolve_path(directory, path, &parent, basename);
    if (result != MOGFS_OK) return result;
    int id = find_entry_id(directory, parent, basename);
    if (id < 0) return MOGFS_ERROR_NOT_FOUND;

    remove_entry[id - 1] = 1;
    for (uint32_t pass = 0; pass < MOGFS_ENTRY_COUNT; pass++) {
        for (uint32_t i = 0; i < MOGFS_ENTRY_COUNT; i++) {
            const unsigned char* entry = const_entry_at(directory, i);
            uint16_t entry_parent =
                read_u16(entry + MOGFS_ENTRY_PARENT_OFFSET);
            if (entry_is_active(entry) && entry_parent &&
                remove_entry[entry_parent - 1U]) {
                remove_entry[i] = 1;
            }
        }
    }

    for (uint32_t i = 0; i < MOGFS_ENTRY_COUNT; i++) {
        if (remove_entry[i]) {
            unsigned char* entry = entry_at(directory, i);
            for (uint32_t j = 0; j < MOGFS_ENTRY_SIZE; j++) entry[j] = 0;
        }
    }
    return write_directory(filesystem, directory);
}

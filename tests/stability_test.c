#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "graphics.h"
#include "rtc.h"
#include "timer.h"
#include "snake.h"
#include "ata.h"
#include "mogfs.h"
#include "heap.h"

struct screen_info screen;

static unsigned char cmos[128];
static unsigned char cmos_register;
static int cmos_update_stuck;
static int ata_selected;
static int ata_present = 1;
static unsigned char ata_status;
static unsigned char ata_command;
static unsigned int ata_data_words;
static unsigned int ata_write_words;
static unsigned int ata_identify_sector_count = 32768;

#define MOCK_DISK_SECTORS (MOGFS_START_LBA + MOGFS_REQUIRED_SECTORS)
struct mock_disk {
    unsigned int sector_count;
    unsigned char sectors[MOCK_DISK_SECTORS][MOGFS_SECTOR_SIZE];
    int fail_read_lba;
    int fail_write_lba;
};

extern volatile unsigned int tick_count;
extern volatile unsigned int uptime_seconds;
extern volatile unsigned int idle_ticks;
extern volatile unsigned int busy_ticks;
extern volatile unsigned int cpu_usage_percent;
extern volatile unsigned int idle_state;

extern int snake_direction_x;
extern int snake_direction_y;
extern int snake_next_direction_x;
extern int snake_next_direction_y;
extern int snake_length;
extern int snake_board_width;
extern int snake_board_height;
extern int snake_game_over;
extern int snake_won;

static void test_heap_boundaries_alignment_and_reuse(void) {
    unsigned char memory[512];
    void* first;
    void* second;
    void* reused;

    assert(heap_init_region(memory + 1, sizeof(memory) - 1));
    assert(kmalloc(0) == 0);
    first = kmalloc(1);
    second = kmalloc(17);
    assert(first && second);
    assert((uintptr_t)first % 8U == 0);
    assert((uintptr_t)second % 8U == 0);

    kfree(first);
    reused = kmalloc(8);
    assert(reused == first);
    kfree(reused);
    kfree(reused);
    kfree((unsigned char*)second + 1);
    kfree(second);

    reused = kmalloc(48);
    assert(reused == first);
    kfree(reused);
}

static void test_heap_coalescing_and_exhaustion(void) {
    unsigned char memory[256];
    void* first;
    void* second;
    void* third;
    unsigned int allocations = 0;

    assert(heap_init_region(memory, sizeof(memory)));
    first = kmalloc(40);
    second = kmalloc(40);
    third = kmalloc(40);
    assert(first && second && third);
    kfree(first);
    kfree(second);
    assert(kmalloc(80) == first);

    assert(heap_init_region(memory, sizeof(memory)));
    while (kmalloc(32)) allocations++;
    assert(allocations > 0);
    assert(kmalloc(1) == 0);
    assert(heap_init_region(memory, sizeof(memory)));
    assert(kmalloc(1));
    assert(!heap_init_region(0, sizeof(memory)));
    assert(!heap_init_region(memory, 1));
    assert(kmalloc(1) == 0);
}

struct test_snake_segment {
    int x;
    int y;
};

extern struct test_snake_segment snake_body[128];
extern void snake_place_food(void);

#define TEST_EDITOR_CAPACITY (MOGFS_FILE_MAX_SIZE + 1U)
extern char editor_buffer[TEST_EDITOR_CAPACITY];
extern char editor_status[48];
extern int editor_length;
extern int editor_cursor;
extern void editor_handle_key(char key);
extern int editor_load_document(void);

void outb(unsigned short port, unsigned char value) {
    if (port == 0x70) cmos_register = value & 0x7F;
    if (port == 0x1F6) {
        ata_selected = 1;
        ata_status = ata_present ? 0x50 : 0;
        ata_data_words = 0;
    }
    if (port == 0x1F7 && ata_selected) {
        ata_data_words = 0;
        ata_command = value;
        ata_status = ata_present ?
                     value == 0x20 || value == 0x30 || value == 0xEC ?
                     0x58 : 0x50 : 0;
    }
}

unsigned char inb(unsigned short port) {
    if (port == 0x1F7 || port == 0x3F6) return ata_status;
    if (port != 0x71) return 0;
    if (cmos_register == 0x0A && cmos_update_stuck) return 0x80;
    return cmos[cmos_register];
}

void outw(unsigned short port, unsigned short value) {
    (void)value;
    if (port == 0x1F0 && ata_status == 0x58) {
        ata_write_words++;
        if (++ata_data_words == 256) ata_status = 0x50;
    }
}

unsigned short inw(unsigned short port) {
    if (port == 0x1F0 && ata_status == 0x58) {
        unsigned int word_index = ata_data_words;
        if (++ata_data_words == 256) ata_status = 0x50;
        if (ata_command == 0xEC && word_index == 60) {
            return (unsigned short)ata_identify_sector_count;
        }
        if (ata_command == 0xEC && word_index == 61) {
            return (unsigned short)(ata_identify_sector_count >> 16);
        }
        return 0;
    }
    return 0;
}

void set_idt_gate(int n, unsigned int handler) {
    (void)n;
    (void)handler;
}

void timer_handler_asm(void) {}

void fill_screen(unsigned char r, unsigned char g, unsigned char b) {
    (void)r; (void)g; (void)b;
}

void draw_rect(int x, int y, int width, int height,
               unsigned char r, unsigned char g, unsigned char b) {
    (void)x; (void)y; (void)width; (void)height;
    (void)r; (void)g; (void)b;
}

void draw_char(int x, int y, char c, unsigned char r,
               unsigned char g, unsigned char b, int scale) {
    (void)x; (void)y; (void)c; (void)r; (void)g; (void)b; (void)scale;
}

void screen_swap(void) {}

static void set_rtc(unsigned char status_b, unsigned char second,
                    unsigned char minute, unsigned char hour,
                    unsigned char day, unsigned char month, unsigned char year) {
    memset(cmos, 0, sizeof(cmos));
    cmos[0x0B] = status_b;
    cmos[0x00] = second;
    cmos[0x02] = minute;
    cmos[0x04] = hour;
    cmos[0x07] = day;
    cmos[0x08] = month;
    cmos[0x09] = year;
    cmos_update_stuck = 0;
}

static void test_rtc_formats_and_failure(void) {
    struct rtc_time value;

    set_rtc(0x02, 0x58, 0x59, 0x23, 0x14, 0x10, 0x26);
    assert(rtc_read(&value));
    assert(value.seconds == 58 && value.minutes == 59 && value.hours == 23);
    assert(value.day == 14 && value.month == 10 && value.year == 26);

    set_rtc(0x06, 58, 59, 23, 14, 10, 26);
    assert(rtc_read(&value));
    assert(value.seconds == 58 && value.minutes == 59 && value.hours == 23);

    set_rtc(0x00, 0x00, 0x01, 0x12, 0x14, 0x10, 0x26);
    assert(rtc_read(&value) && value.hours == 0);
    cmos[0x04] = 0x91;
    assert(rtc_read(&value) && value.hours == 23);

    cmos[0x02] = 0x6A;
    value.minutes = 99;
    assert(!rtc_read(&value));
    assert(value.seconds == 0 && value.minutes == 0 && value.hours == 0);

    set_rtc(0x02, 0, 0, 0, 1, 1, 26);
    cmos_update_stuck = 1;
    assert(!rtc_read(&value));
    assert(value.day == 0 && value.month == 0 && value.year == 0);
    assert(!rtc_read(0));
}

static void test_timer_counters(void) {
    tick_count = uptime_seconds = idle_ticks = busy_ticks = cpu_usage_percent = 0;
    for (unsigned int i = 0; i < 100; i++) {
        timer_set_idle_state(i < 50);
        timer_handler_c();
    }
    assert(timer_get_ticks() == 100);
    assert(timer_get_uptime_seconds() == 1);
    assert(timer_get_cpu_usage_percent() == 50);

    tick_count = 0xFFFFFFCEU;
    uptime_seconds = idle_ticks = busy_ticks = cpu_usage_percent = 0;
    for (unsigned int i = 0; i < 100; i++) {
        timer_set_idle_state(i < 50);
        timer_handler_c();
    }
    assert(timer_get_ticks() == 50);
    assert(timer_get_uptime_seconds() == 1);
    assert(timer_get_cpu_usage_percent() == 50);
}

static void test_snake_queued_turns(void) {
    screen.width = 320;
    screen.height = 240;
    tick_count = 0;
    snake_start();

    snake_key_handler('w');
    assert(snake_next_direction_x == 0 && snake_next_direction_y == -1);

    snake_key_handler('a');
    assert(snake_next_direction_x == 0 && snake_next_direction_y == -1);

    snake_key_handler('s');
    assert(snake_next_direction_x == 0 && snake_next_direction_y == -1);

    snake_key_handler('d');
    assert(snake_next_direction_x == 1 && snake_next_direction_y == 0);
}

static void test_snake_full_board_finishes_without_hanging(void) {
    snake_board_width = 2;
    snake_board_height = 2;
    snake_length = 4;
    snake_game_over = 0;
    snake_won = 0;
    snake_body[0] = (struct test_snake_segment){0, 0};
    snake_body[1] = (struct test_snake_segment){1, 0};
    snake_body[2] = (struct test_snake_segment){0, 1};
    snake_body[3] = (struct test_snake_segment){1, 1};

    snake_place_food();
    assert(snake_game_over && snake_won);
}

static void test_editor_size_limit_and_unsaved_status(void) {
    screen.width = 1024;
    screen.height = 768;
    editor_length = TEST_EDITOR_CAPACITY - 1U;
    editor_cursor = editor_length;
    memset(editor_buffer, 'x', editor_length);
    editor_buffer[editor_length] = '\0';

    editor_handle_key('y');
    assert(editor_length == TEST_EDITOR_CAPACITY - 1U &&
           editor_cursor == TEST_EDITOR_CAPACITY - 1U);
    assert(editor_buffer[TEST_EDITOR_CAPACITY - 2U] == 'x' &&
           editor_buffer[TEST_EDITOR_CAPACITY - 1U] == '\0');
    assert(strcmp(editor_status, "Document full; delete text to continue") == 0);

    editor_handle_key('\b');
    assert(editor_length == TEST_EDITOR_CAPACITY - 2U &&
           editor_cursor == TEST_EDITOR_CAPACITY - 2U);
    assert(strcmp(editor_status, "Unsaved changes") == 0);
}

static void test_editor_vertical_navigation(void) {
    const char text[] = "one\ntwo\nthree";
    memcpy(editor_buffer, text, sizeof(text));
    editor_length = (int)sizeof(text) - 1;
    editor_cursor = 1;

    editor_handle_key(2);
    assert(editor_cursor == 5);
    editor_handle_key(2);
    assert(editor_cursor == 9);
    editor_handle_key(1);
    assert(editor_cursor == 5);

    int position = 0;
    for (int line = 0; line < 80; line++) {
        for (int column = 0; column < 24; column++) {
            editor_buffer[position++] = 'a';
        }
        if (line < 79) editor_buffer[position++] = '\n';
    }
    editor_buffer[position] = '\0';
    editor_length = position;
    editor_cursor = 10 * 25 + 17;
    for (int line = 0; line < 50; line++) editor_handle_key(2);
    assert(editor_cursor == 60 * 25 + 17);
    for (int line = 0; line < 100; line++) editor_handle_key(2);
    assert(editor_cursor == 79 * 25 + 17);
}

static void test_editor_empty_save_slot(void) {
    editor_buffer[0] = '\0';
    editor_length = 0;
    editor_cursor = 0;
    assert(editor_load_document() == 0);
    assert(editor_length == 0 && editor_cursor == 0);
}

static void test_editor_disk_failures_preserve_document(void) {
    memcpy(editor_buffer, "keep", 5);
    editor_length = 4;
    editor_cursor = 4;
    ata_present = 0;

    assert(editor_load_document() == -1);
    assert(editor_length == 4 && editor_cursor == 4);
    assert(strcmp(editor_buffer, "keep") == 0);

    editor_handle_key(15);
    assert(strcmp(editor_status, "Disk read failed; editor unchanged") == 0);
    assert(editor_length == 4 && strcmp(editor_buffer, "keep") == 0);

    editor_handle_key(20);
    assert(strcmp(editor_status, "Legacy save failed: disk I/O error") == 0);
    ata_present = 1;
}

static void test_ata_selects_device_before_polling(void) {
    unsigned char sector[512] = {0};
    ata_selected = 0;
    ata_status = 0;
    ata_data_words = 0;
    ata_write_words = 0;

    assert(ata_write_sector(1, sector));
    assert(ata_selected);
    assert(ata_write_words == 256);
}

static void test_ata_sector_count_initializes_filesystem_device(void) {
    struct mogfs_block_device device = {0};
    unsigned int sectors = 0;

    ata_selected = 0;
    ata_present = 1;
    ata_status = 0;
    assert(ata_get_sector_count(&sectors));
    assert(sectors == ata_identify_sector_count);
    assert(mogfs_ata_device_init(&device));
    assert(device.sector_count == ata_identify_sector_count);
    assert(device.read_sector != 0 && device.write_sector != 0);

    ata_present = 0;
    sectors = 123;
    assert(!ata_get_sector_count(&sectors));
    assert(sectors == 123);
    assert(!mogfs_ata_device_init(&device));
    ata_present = 1;
}

static int mock_read_sector(void* context, unsigned int lba,
                            unsigned char* buffer) {
    struct mock_disk* disk = context;
    if (!buffer || lba >= disk->sector_count ||
        (int)lba == disk->fail_read_lba) {
        return 0;
    }
    memcpy(buffer, disk->sectors[lba], MOGFS_SECTOR_SIZE);
    return 1;
}

static int mock_write_sector(void* context, unsigned int lba,
                             unsigned char* buffer) {
    struct mock_disk* disk = context;
    if (!buffer || lba >= disk->sector_count ||
        (int)lba == disk->fail_write_lba) {
        return 0;
    }
    memcpy(disk->sectors[lba], buffer, MOGFS_SECTOR_SIZE);
    return 1;
}

static unsigned int test_crc32(const unsigned char* data, unsigned int size) {
    unsigned int crc = 0xFFFFFFFFU;
    for (unsigned int i = 0; i < size; i++) {
        crc ^= data[i];
        for (unsigned int bit = 0; bit < 8; bit++) {
            unsigned int mask = 0U - (crc & 1U);
            crc = (crc >> 1) ^ (0xEDB88320U & mask);
        }
    }
    return ~crc;
}

static void store_u32(unsigned char* buffer, unsigned int value) {
    buffer[0] = (unsigned char)value;
    buffer[1] = (unsigned char)(value >> 8);
    buffer[2] = (unsigned char)(value >> 16);
    buffer[3] = (unsigned char)(value >> 24);
}

static void test_mogfs_storage_and_file_operations(void) {
    static struct mock_disk disk;
    struct mogfs_block_device device = {
        &disk, MOCK_DISK_SECTORS, mock_read_sector, mock_write_sector
    };
    struct mogfs filesystem;
    struct mogfs_file_info files[MOGFS_ENTRY_COUNT];
    unsigned char content[700];
    unsigned char maximum_content[MOGFS_FILE_MAX_SIZE];
    unsigned char read_buffer[MOGFS_FILE_MAX_SIZE];
    unsigned int count = 99;
    unsigned int read_size = 0;

    memset(&disk, 0xA5, sizeof(disk));
    disk.sector_count = MOCK_DISK_SECTORS;
    disk.fail_read_lba = -1;
    disk.fail_write_lba = -1;
    for (unsigned int lba = 1; lba <= 5; lba++) {
        memset(disk.sectors[lba], 0x5A, MOGFS_SECTOR_SIZE);
    }

    assert(mogfs_mount(&filesystem, &device) == MOGFS_ERROR_CORRUPT);
    struct mogfs_block_device undersized = device;
    undersized.sector_count--;
    assert(mogfs_format(&undersized) == MOGFS_ERROR_INVALID_ARGUMENT);
    assert(mogfs_format(&device) == MOGFS_OK);
    for (unsigned int lba = 1; lba <= 5; lba++) {
        for (unsigned int i = 0; i < MOGFS_SECTOR_SIZE; i++) {
            assert(disk.sectors[lba][i] == 0x5A);
        }
    }
    for (unsigned int lba = 6; lba < MOGFS_START_LBA; lba++) {
        assert(disk.sectors[lba][0] == 0xA5);
    }
    assert(disk.sectors[MOGFS_START_LBA + 1U +
                        MOGFS_DIRECTORY_BANK_COUNT *
                        MOGFS_DIRECTORY_SECTORS][0] == 0xA5);

    assert(mogfs_mount(&filesystem, &undersized) ==
           MOGFS_ERROR_INVALID_ARGUMENT);
    assert(mogfs_mount(&filesystem, &device) == MOGFS_OK);
    assert(mogfs_list(&filesystem, "/", files, MOGFS_ENTRY_COUNT, &count) ==
           MOGFS_OK);
    assert(count == 0);

    assert(mogfs_create_file(&filesystem, "/", "bad/name") ==
           MOGFS_ERROR_INVALID_ARGUMENT);
    assert(mogfs_create_file(&filesystem, "/", "..") ==
           MOGFS_ERROR_INVALID_ARGUMENT);
    assert(mogfs_create_file(&filesystem, "/",
                        "this_name_is_longer_than_thirty_one") ==
           MOGFS_ERROR_INVALID_ARGUMENT);
    char maximum_name[MOGFS_FILE_NAME_SIZE] = "1234567890123456789012345678901";
    assert(mogfs_create_file(&filesystem, "/", maximum_name) == MOGFS_OK);
    assert(mogfs_delete(&filesystem,
                        "/1234567890123456789012345678901") == MOGFS_OK);
    assert(mogfs_create_file(&filesystem, "/", "notes.txt") == MOGFS_OK);
    assert(mogfs_create_file(&filesystem, "/", "notes.txt") ==
           MOGFS_ERROR_ALREADY_EXISTS);
    assert(mogfs_create_directory(&filesystem, "/", "Documents") == MOGFS_OK);
    assert(mogfs_create_directory(&filesystem, "/Documents", "Work") ==
           MOGFS_OK);
    assert(mogfs_create_file(&filesystem, "/Documents/Work", "notes.txt") ==
           MOGFS_OK);
    assert(mogfs_create_file(&filesystem, "/missing", "file.txt") ==
           MOGFS_ERROR_NOT_FOUND);
    for (unsigned int i = 0; i < sizeof(content); i++) {
        content[i] = (unsigned char)(i * 17U);
    }
    assert(mogfs_write(&filesystem, "/Documents/Work/notes.txt", content,
                       sizeof(content)) == MOGFS_OK);
    assert(mogfs_read(&filesystem, "/Documents/Work/notes.txt", read_buffer,
                      sizeof(read_buffer), &read_size) == MOGFS_OK);
    assert(read_size == sizeof(content));
    assert(memcmp(content, read_buffer, sizeof(content)) == 0);
    assert(mogfs_list(&filesystem, "/Documents/Work", files, MOGFS_ENTRY_COUNT,
                      &count) == MOGFS_OK);
    assert(count == 1 && strcmp(files[0].name, "notes.txt") == 0);
    assert(mogfs_delete(&filesystem, "/Documents") == MOGFS_OK);
    assert(mogfs_read(&filesystem, "/Documents/Work/notes.txt", read_buffer,
                      sizeof(read_buffer), &read_size) == MOGFS_ERROR_NOT_FOUND);
    assert(mogfs_list(&filesystem, "/", files, MOGFS_ENTRY_COUNT, &count) ==
           MOGFS_OK);
    assert(count == 1 && strcmp(files[0].name, "notes.txt") == 0);
    assert(mogfs_write(&filesystem, "/notes.txt", content, sizeof(content)) ==
           MOGFS_OK);
    assert(mogfs_read(&filesystem, "/notes.txt", read_buffer,
                      sizeof(read_buffer), &read_size) == MOGFS_OK);
    assert(read_size == sizeof(content));
    assert(memcmp(content, read_buffer, sizeof(content)) == 0);

    memset(read_buffer, 0xCC, sizeof(read_buffer));
    read_size = 1234;
    assert(mogfs_read(&filesystem, "/notes.txt", read_buffer, 10, &read_size) ==
           MOGFS_ERROR_BUFFER_TOO_SMALL);
    assert(read_size == 1234 && read_buffer[0] == 0xCC);
    assert(mogfs_write(&filesystem, "/notes.txt", content,
                       MOGFS_FILE_MAX_SIZE + 1U) == MOGFS_ERROR_TOO_LARGE);

    disk.fail_write_lba = MOGFS_START_LBA + 1U +
        MOGFS_DIRECTORY_BANK_COUNT * MOGFS_DIRECTORY_SECTORS + 1U;
    assert(mogfs_write(&filesystem, "/notes.txt", content, sizeof(content)) ==
           MOGFS_ERROR_IO);
    disk.fail_write_lba = -1;
    assert(mogfs_read(&filesystem, "/notes.txt", read_buffer,
                      sizeof(read_buffer), &read_size) == MOGFS_OK);
    assert(read_size == sizeof(content));
    assert(memcmp(content, read_buffer, sizeof(content)) == 0);

    disk.fail_read_lba = MOGFS_START_LBA + 1U +
        MOGFS_DIRECTORY_BANK_COUNT * MOGFS_DIRECTORY_SECTORS +
        MOGFS_FILE_MAX_SIZE / MOGFS_SECTOR_SIZE + 1U;
    read_size = 1234;
    assert(mogfs_read(&filesystem, "/notes.txt", read_buffer,
                      sizeof(read_buffer), &read_size) == MOGFS_ERROR_IO);
    assert(read_size == 1234);
    disk.fail_read_lba = -1;

    disk.fail_write_lba = MOGFS_START_LBA + 1U +
        (1U - filesystem.active_directory_bank) * MOGFS_DIRECTORY_SECTORS + 1U;
    assert(mogfs_create_file(&filesystem, "/", "not-saved") ==
           MOGFS_ERROR_IO);
    disk.fail_write_lba = -1;
    disk.fail_write_lba = MOGFS_START_LBA;
    assert(mogfs_create_file(&filesystem, "/", "not-committed") ==
           MOGFS_ERROR_IO);
    disk.fail_write_lba = -1;
    assert(mogfs_mount(&filesystem, &device) == MOGFS_OK);
    assert(mogfs_list(&filesystem, "/", files, MOGFS_ENTRY_COUNT, &count) ==
           MOGFS_OK);
    assert(count == 1 && strcmp(files[0].name, "notes.txt") == 0);

    for (unsigned int i = 1; i < MOGFS_FILE_COUNT; i++) {
        char name[8] = "file0";
        name[4] = (char)('0' + i);
        assert(mogfs_create_file(&filesystem, "/", name) == MOGFS_OK);
    }
    assert(mogfs_create_file(&filesystem, "/", "full") ==
           MOGFS_ERROR_NO_SPACE);
    assert(mogfs_list(&filesystem, "/", files, MOGFS_ENTRY_COUNT, &count) ==
           MOGFS_OK);
    assert(count == MOGFS_FILE_COUNT);
    assert(mogfs_list(&filesystem, "/", files, 1, &count) ==
           MOGFS_ERROR_BUFFER_TOO_SMALL);

    for (unsigned int i = 0; i < sizeof(maximum_content); i++) {
        maximum_content[i] = (unsigned char)(i * 29U);
    }
    assert(mogfs_write(&filesystem, "/file1", maximum_content,
                       sizeof(maximum_content)) == MOGFS_OK);
    assert(mogfs_read(&filesystem, "/file1", read_buffer,
                      sizeof(read_buffer), &read_size) == MOGFS_OK);
    assert(read_size == sizeof(maximum_content));
    assert(memcmp(maximum_content, read_buffer, sizeof(maximum_content)) == 0);

    assert(mogfs_write(&filesystem, "/file2", 0, 0) == MOGFS_OK);
    assert(mogfs_read(&filesystem, "/file2", 0, 0, &read_size) == MOGFS_OK);
    assert(read_size == 0);

    assert(mogfs_mount(&filesystem, &device) == MOGFS_OK);
    assert(mogfs_read(&filesystem, "/notes.txt", read_buffer,
                      sizeof(read_buffer), &read_size) == MOGFS_OK);
    assert(read_size == sizeof(content));
    assert(memcmp(content, read_buffer, sizeof(content)) == 0);

    disk.fail_write_lba = MOGFS_START_LBA + 1U +
        (1U - filesystem.active_directory_bank) * MOGFS_DIRECTORY_SECTORS + 1U;
    assert(mogfs_write(&filesystem, "/notes.txt", content, 1) ==
           MOGFS_ERROR_IO);
    disk.fail_write_lba = -1;
    assert(mogfs_read(&filesystem, "/notes.txt", read_buffer,
                      sizeof(read_buffer), &read_size) == MOGFS_OK);
    assert(read_size == sizeof(content));
    assert(memcmp(content, read_buffer, sizeof(content)) == 0);

    unsigned int active_directory_lba = MOGFS_START_LBA + 1U +
        filesystem.active_directory_bank * MOGFS_DIRECTORY_SECTORS;
    disk.sectors[active_directory_lba][0] ^= 1;
    assert(mogfs_list(&filesystem, "/", files, MOGFS_ENTRY_COUNT, &count) ==
           MOGFS_ERROR_CORRUPT);
    disk.sectors[active_directory_lba][0] ^= 1;

    assert(mogfs_delete(&filesystem, "/file2") == MOGFS_OK);
    assert(mogfs_delete(&filesystem, "/missing") == MOGFS_ERROR_NOT_FOUND);
    assert(mogfs_create_file(&filesystem, "/", "replacement") == MOGFS_OK);

    disk.sectors[MOGFS_START_LBA][8] = 3;
    store_u32(disk.sectors[MOGFS_START_LBA] + 508,
              test_crc32(disk.sectors[MOGFS_START_LBA], 508));
    assert(mogfs_mount(&filesystem, &device) == MOGFS_ERROR_UNSUPPORTED);
    disk.sectors[MOGFS_START_LBA][8] = 2;
    store_u32(disk.sectors[MOGFS_START_LBA] + 508,
              test_crc32(disk.sectors[MOGFS_START_LBA], 508));
    assert(mogfs_mount(&filesystem, &device) == MOGFS_OK);
}

static void test_mogfs_reports_disk_capacity_and_io_errors(void) {
    static struct mock_disk disk;
    struct mogfs_block_device device = {
        &disk, MOCK_DISK_SECTORS, mock_read_sector, mock_write_sector
    };
    struct mogfs filesystem;
    unsigned char content[MOGFS_FILE_MAX_SIZE] = {0x3C};

    memset(&disk, 0, sizeof(disk));
    disk.sector_count = MOCK_DISK_SECTORS;
    disk.fail_read_lba = -1;
    disk.fail_write_lba = -1;
    assert(mogfs_format(&device) == MOGFS_OK);
    assert(mogfs_mount(&filesystem, &device) == MOGFS_OK);
    for (unsigned int i = 0; i < MOGFS_FILE_COUNT; i++) {
        char name[8] = "file0";
        name[4] = (char)('0' + i);
        assert(mogfs_create_file(&filesystem, "/", name) == MOGFS_OK);
    }

    disk.sector_count--;
    assert(mogfs_write(&filesystem, "/file7", content, sizeof(content)) ==
           MOGFS_ERROR_IO);
    disk.sector_count = MOCK_DISK_SECTORS;
    unsigned char read_buffer[MOGFS_FILE_MAX_SIZE];
    unsigned int read_size = 1;
    assert(mogfs_read(&filesystem, "/file7", read_buffer, sizeof(read_buffer),
                      &read_size) == MOGFS_OK);
    assert(read_size == 0);

    disk.fail_read_lba = MOGFS_START_LBA;
    assert(mogfs_mount(&filesystem, &device) == MOGFS_ERROR_IO);
}

int main(void) {
    test_heap_boundaries_alignment_and_reuse();
    test_heap_coalescing_and_exhaustion();
    test_rtc_formats_and_failure();
    test_timer_counters();
    test_snake_queued_turns();
    test_snake_full_board_finishes_without_hanging();
    test_editor_size_limit_and_unsaved_status();
    test_editor_vertical_navigation();
    test_editor_empty_save_slot();
    test_editor_disk_failures_preserve_document();
    test_ata_selects_device_before_polling();
    test_ata_sector_count_initializes_filesystem_device();
    test_mogfs_storage_and_file_operations();
    test_mogfs_reports_disk_capacity_and_io_errors();
    puts("MogOS stability tests passed");
    return 0;
}

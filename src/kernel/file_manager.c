#include "file_manager.h"
#include "desktop.h"
#include "graphics.h"
#include "mogfs.h"

#define FILE_MANAGER_MAX_VISIBLE 48
#define FILE_MANAGER_INPUT_NEW_FILE 1
#define FILE_MANAGER_INPUT_NEW_DIRECTORY 2
#define FILE_MANAGER_CONFIRM_FORMAT 3
#define FILE_MANAGER_CONFIRM_DELETE 4
#define FILE_MANAGER_CONFIRM_OVERWRITE 5
#define FILE_MANAGER_CONFIRM_DISCARD 6

static struct mogfs filesystem;
static struct mogfs_block_device block_device;
static struct mogfs_file_info entries[MOGFS_ENTRY_COUNT];
static uint32_t entry_count;
static uint32_t selected_entry;
static uint32_t scroll_offset;
static int manager_mode;
static int input_mode;
static char current_directory[MOGFS_PATH_SIZE];
static char input_buffer[MOGFS_FILE_NAME_SIZE];
static uint32_t input_length;
static char pending_path[MOGFS_PATH_SIZE];
static char manager_status[72];

extern void (*current_app_key_handler)(char key);
extern int editor_open_file(const char* path);
extern int editor_save_file(const char* path);
extern int editor_has_unsaved_changes(void);
extern void run_text_editor(void);

static void set_status(const char* text) {
    uint32_t i = 0;
    while (text[i] && i < sizeof(manager_status) - 1) {
        manager_status[i] = text[i];
        i++;
    }
    manager_status[i] = '\0';
}

static void draw_text(int x, int y, const char* text,
                      unsigned char r, unsigned char g, unsigned char b) {
    for (uint32_t i = 0; text[i] && x + (int)i * 9 < screen.width; i++) {
        draw_char(x + (int)i * 9, y, text[i], r, g, b, 1);
    }
}

static void append_unsigned(char* buffer, uint32_t* length, uint32_t value) {
    char digits[10];
    uint32_t count = 0;
    do {
        digits[count++] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value);
    while (count) buffer[(*length)++] = digits[--count];
    buffer[*length] = '\0';
}

static uint32_t visible_rows(void) {
    uint32_t rows = screen.height > 90 ? (screen.height - 85U) / 12U : 1U;
    if (rows > FILE_MANAGER_MAX_VISIBLE) rows = FILE_MANAGER_MAX_VISIBLE;
    if (rows == 0) rows = 1;
    return rows;
}

static void ensure_selection_visible(void) {
    uint32_t rows = visible_rows();
    if (selected_entry < scroll_offset) scroll_offset = selected_entry;
    if (selected_entry >= scroll_offset + rows) {
        scroll_offset = selected_entry - rows + 1U;
    }
}

static void refresh_entries(void) {
    int result = mogfs_list(&filesystem, current_directory, entries,
                            MOGFS_ENTRY_COUNT, &entry_count);
    if (result != MOGFS_OK) {
        entry_count = 0;
        set_status(result == MOGFS_ERROR_IO ? "Disk I/O error listing folder" :
                   result == MOGFS_ERROR_CORRUPT ? "Filesystem metadata is corrupt" :
                   "Cannot read this folder");
        return;
    }
    if (entry_count == 0) selected_entry = 0;
    else if (selected_entry >= entry_count) selected_entry = entry_count - 1U;
    ensure_selection_visible();
}

static int append_path(char* destination, const char* directory,
                       const char* name) {
    uint32_t length = 0;
    while (directory[length] && length < MOGFS_PATH_SIZE) {
        destination[length] = directory[length];
        length++;
    }
    if (length >= MOGFS_PATH_SIZE - 1U) return 0;
    if (length > 1 && destination[length - 1U] != '/') {
        destination[length++] = '/';
    }
    for (uint32_t i = 0; name[i]; i++) {
        if (length >= MOGFS_PATH_SIZE - 1U) return 0;
        destination[length++] = name[i];
    }
    destination[length] = '\0';
    return 1;
}

static void draw_manager(void) {
    if (!app_running || current_app_key_handler != file_manager_handle_key) {
        return;
    }
    fill_screen(0x10, 0x10, 0x10);
    draw_rect(0, 0, screen.width, 10, 0, 0, 0);
    if (manager_mode == FILE_MANAGER_SAVE) {
        draw_text(4, 1, "SAVE TEXT FILE", 0xFF, 0xFF, 0x00);
    } else if (manager_mode == FILE_MANAGER_OPEN) {
        draw_text(4, 1, "OPEN TEXT FILE", 0xFF, 0xFF, 0x00);
    } else {
        draw_text(4, 1, "FILE EXPLORER", 0xFF, 0xFF, 0x00);
    }

    draw_text(4, 15, current_directory, 0x00, 0xFF, 0x70);
    uint32_t rows = visible_rows();
    for (uint32_t row = 0; row < rows && scroll_offset + row < entry_count; row++) {
        uint32_t index = scroll_offset + row;
        struct mogfs_file_info* entry = &entries[index];
        int y = 30 + (int)row * 12;
        if (index == selected_entry) {
            draw_rect(2, y - 1, screen.width - 4, 10, 0x30, 0x30, 0x00);
        }
        if (entry->is_directory) {
            draw_text(8, y, "[DIR] ", 0x00, 0xD0, 0xFF);
        } else {
            draw_text(8, y, "[TXT] ", 0xD0, 0xD0, 0xD0);
        }
        draw_text(62, y, entry->name, 0xE0, 0xE0, 0xE0);
        if (!entry->is_directory) {
            char size_text[16] = " (";
            uint32_t length = 2;
            append_unsigned(size_text, &length, entry->size);
            size_text[length++] = ')';
            size_text[length] = '\0';
            uint32_t name_length = 0;
            while (entry->name[name_length]) name_length++;
            draw_text(62 + (int)name_length * 9, y, size_text,
                      0x88, 0x88, 0x88);
        }
    }

    int status_y = screen.height > 36 ? screen.height - 34 : 0;
    draw_text(4, status_y, manager_status, 0xFF, 0xA0, 0x40);
    draw_text(4, status_y + 12,
              "Enter open  Backspace up  N new .txt  D folder  X delete  Ctrl+H home",
              0x88, 0x88, 0x88);

    if (input_mode == FILE_MANAGER_INPUT_NEW_FILE ||
        input_mode == FILE_MANAGER_INPUT_NEW_DIRECTORY) {
        draw_rect(16, screen.height / 2 - 25, screen.width - 32, 54,
                  0x00, 0x00, 0x00);
        draw_text(24, screen.height / 2 - 18,
                  input_mode == FILE_MANAGER_INPUT_NEW_FILE ?
                  "New text file name:" : "New folder name:",
                  0xFF, 0xFF, 0x00);
        draw_text(24, screen.height / 2,
                  input_buffer, 0xFF, 0xFF, 0xFF);
        draw_text(24, screen.height / 2 + 12,
                  "Enter create, Esc cancel", 0x88, 0x88, 0x88);
    } else if (input_mode == FILE_MANAGER_CONFIRM_FORMAT ||
               input_mode == FILE_MANAGER_CONFIRM_DELETE ||
               input_mode == FILE_MANAGER_CONFIRM_OVERWRITE ||
               input_mode == FILE_MANAGER_CONFIRM_DISCARD) {
        draw_rect(16, screen.height / 2 - 24, screen.width - 32, 48,
                  0x00, 0x00, 0x00);
        draw_text(24, screen.height / 2 - 16, manager_status,
                  0xFF, 0xA0, 0x40);
        draw_text(24, screen.height / 2 + 2, "Y confirm, N/Esc cancel",
                  0xFF, 0xFF, 0xFF);
    }
    screen_swap();
}

static int mount_filesystem(void) {
    filesystem.mounted = 0;
    if (!mogfs_ata_device_init(&block_device)) {
        set_status("No supported ATA disk detected");
        return 0;
    }
    int result = mogfs_mount(&filesystem, &block_device);
    if (result == MOGFS_OK) {
        set_status("Filesystem ready");
        refresh_entries();
        return 1;
    }
    if (result == MOGFS_ERROR_INVALID_ARGUMENT) {
        set_status("Disk is too small for MogFS");
    } else if (result == MOGFS_ERROR_IO) {
        set_status("Disk read failed");
    } else if (result == MOGFS_ERROR_UNSUPPORTED) {
        set_status("Unsupported MogFS version; X then Y to reformat");
    } else {
        set_status("No MogFS volume; X then Y to format reserved area");
    }
    return 0;
}

static int ensure_mounted(void) {
    if (filesystem.mounted) return MOGFS_OK;
    if (!mogfs_ata_device_init(&block_device)) return MOGFS_ERROR_IO;
    return mogfs_mount(&filesystem, &block_device);
}

int file_manager_read_file(const char* path, unsigned char* buffer,
                           uint32_t capacity, uint32_t* size) {
    int result = ensure_mounted();
    if (result != MOGFS_OK) return result;
    return mogfs_read(&filesystem, path, buffer, capacity, size);
}

int file_manager_write_file(const char* path, const unsigned char* buffer,
                            uint32_t size) {
    int result = ensure_mounted();
    if (result != MOGFS_OK) return result;
    return mogfs_write(&filesystem, path, buffer, size);
}

static void finish_open(const char* path) {
    if (editor_open_file(path)) return;
    set_status("Open failed; document unchanged");
    refresh_entries();
    draw_manager();
}

static void finish_save(const char* path) {
    if (editor_save_file(path)) return;
    set_status("Save failed; check disk space and I/O");
    refresh_entries();
    draw_manager();
}

static void begin_name_input(int mode) {
    input_mode = mode;
    input_length = 0;
    input_buffer[0] = '\0';
}

static void commit_new_entry(void) {
    char name[MOGFS_FILE_NAME_SIZE];
    uint32_t length = input_length;
    for (uint32_t i = 0; i <= length; i++) name[i] = input_buffer[i];

    if (input_mode == FILE_MANAGER_INPUT_NEW_FILE) {
        int has_txt_suffix = length >= 4 &&
            name[length - 4] == '.' &&
            name[length - 3] == 't' &&
            name[length - 2] == 'x' &&
            name[length - 1] == 't';
        if (!has_txt_suffix) {
            if (length + 4 >= MOGFS_FILE_NAME_SIZE) {
                set_status("Name too long for .txt extension");
                input_mode = 0;
                return;
            }
            name[length++] = '.';
            name[length++] = 't';
            name[length++] = 'x';
            name[length++] = 't';
            name[length] = '\0';
        }
    }

    int result;
    if (input_mode == FILE_MANAGER_INPUT_NEW_DIRECTORY) {
        result = mogfs_create_directory(&filesystem, current_directory, name);
    } else {
        result = mogfs_create_file(&filesystem, current_directory, name);
    }
    input_mode = 0;
    if (result != MOGFS_OK) {
        set_status(result == MOGFS_ERROR_ALREADY_EXISTS ? "That name already exists" :
                   result == MOGFS_ERROR_NO_SPACE ? "Filesystem is full" :
                   result == MOGFS_ERROR_INVALID_ARGUMENT ? "Invalid name" :
                   "Create failed; disk I/O error");
        return;
    }

    char path[MOGFS_PATH_SIZE];
    if (!append_path(path, current_directory, name)) {
        set_status("Path is too long");
        return;
    }
    if (manager_mode == FILE_MANAGER_SAVE) {
        finish_save(path);
        return;
    }
    set_status(input_length ? "Created" : "Created");
    refresh_entries();
    draw_manager();
}

static void go_to_parent(void) {
    uint32_t length = 0;
    while (current_directory[length]) length++;
    if (length <= 1) return;
    uint32_t slash = length - 1U;
    while (slash > 0 && current_directory[slash] != '/') slash--;
    if (slash == 0) {
        current_directory[1] = '\0';
    } else {
        current_directory[slash] = '\0';
    }
    selected_entry = 0;
    scroll_offset = 0;
    refresh_entries();
}

static void enter_selected(void) {
    if (selected_entry >= entry_count) return;
    struct mogfs_file_info* entry = &entries[selected_entry];
    char path[MOGFS_PATH_SIZE];
    if (!append_path(path, current_directory, entry->name)) {
        set_status("Path is too long");
        return;
    }
    if (entry->is_directory) {
        uint32_t i = 0;
        while (path[i]) {
            current_directory[i] = path[i];
            i++;
        }
        current_directory[i] = '\0';
        selected_entry = 0;
        scroll_offset = 0;
        refresh_entries();
        return;
    }
    if (manager_mode == FILE_MANAGER_SAVE) {
        input_mode = FILE_MANAGER_CONFIRM_OVERWRITE;
        uint32_t i = 0;
        while (path[i]) {
            pending_path[i] = path[i];
            i++;
        }
        pending_path[i] = '\0';
        set_status("Overwrite the selected text file?");
        return;
    }
    if (manager_mode == FILE_MANAGER_OPEN ||
        manager_mode == FILE_MANAGER_BROWSE) {
        if (editor_has_unsaved_changes()) {
            input_mode = FILE_MANAGER_CONFIRM_DISCARD;
            uint32_t i = 0;
            while (path[i]) {
                pending_path[i] = path[i];
                i++;
            }
            pending_path[i] = '\0';
            set_status("Discard unsaved editor changes?");
            return;
        }
        finish_open(path);
    }
}

void file_manager_start(int mode) {
    manager_mode = mode;
    input_mode = 0;
    selected_entry = 0;
    scroll_offset = 0;
    current_directory[0] = '/';
    current_directory[1] = '\0';
    entry_count = 0;
    app_running = 1;
    current_app_key_handler = file_manager_handle_key;
    mount_filesystem();
    draw_manager();
}

void file_manager_handle_key(char key) {
    if (input_mode == FILE_MANAGER_INPUT_NEW_FILE ||
        input_mode == FILE_MANAGER_INPUT_NEW_DIRECTORY) {
        if (key == 27) {
            input_mode = 0;
            set_status("Cancelled");
        } else if (key == '\b') {
            if (input_length) input_buffer[--input_length] = '\0';
        } else if (key == '\n') {
            if (input_length) commit_new_entry();
            else {
                input_mode = 0;
                set_status("Name cannot be empty");
            }
        } else if (key >= 32 && key < 127 &&
                   input_length < MOGFS_FILE_NAME_SIZE - 1U) {
            input_buffer[input_length++] = key;
            input_buffer[input_length] = '\0';
        }
        draw_manager();
        return;
    }

    if (input_mode == FILE_MANAGER_CONFIRM_FORMAT) {
        if (key == 'y' || key == 'Y') {
            int result = mogfs_format(&block_device);
            input_mode = 0;
            if (result == MOGFS_OK && mogfs_mount(&filesystem, &block_device) == MOGFS_OK) {
                set_status("MogFS formatted; old LBA 1-5 save is untouched");
                refresh_entries();
            } else {
                set_status("Format failed; disk I/O error");
            }
        } else if (key == 'n' || key == 'N' || key == 27) {
            input_mode = 0;
            set_status("Format cancelled");
        }
        draw_manager();
        return;
    }

    if (input_mode == FILE_MANAGER_CONFIRM_DELETE ||
        input_mode == FILE_MANAGER_CONFIRM_OVERWRITE ||
        input_mode == FILE_MANAGER_CONFIRM_DISCARD) {
        int confirm = key == 'y' || key == 'Y';
        int cancel = key == 'n' || key == 'N' || key == 27;
        if (confirm) {
            int mode = input_mode;
            input_mode = 0;
            if (mode == FILE_MANAGER_CONFIRM_DELETE) {
                char path[MOGFS_PATH_SIZE];
                if (selected_entry < entry_count &&
                    append_path(path, current_directory,
                                entries[selected_entry].name)) {
                    int result = mogfs_delete(&filesystem, path);
                    set_status(result == MOGFS_OK ? "Deleted" :
                               "Delete failed");
                    refresh_entries();
                }
            } else if (mode == FILE_MANAGER_CONFIRM_OVERWRITE) {
                finish_save(pending_path);
            } else {
                finish_open(pending_path);
            }
        } else if (cancel) {
            input_mode = 0;
            set_status("Cancelled");
        }
        draw_manager();
        return;
    }

    if (key == 1) {
        if (selected_entry) selected_entry--;
        ensure_selection_visible();
    } else if (key == 2) {
        if (selected_entry + 1U < entry_count) selected_entry++;
        ensure_selection_visible();
    } else if (key == 3 || key == '\b') {
        go_to_parent();
    } else if (key == '\n') {
        enter_selected();
    } else if (key == 'n' || key == 'N') {
        begin_name_input(FILE_MANAGER_INPUT_NEW_FILE);
    } else if (key == 'd' || key == 'D') {
        begin_name_input(FILE_MANAGER_INPUT_NEW_DIRECTORY);
    } else if (key == 'x' || key == 'X') {
        if (filesystem.mounted && selected_entry < entry_count) {
            input_mode = FILE_MANAGER_CONFIRM_DELETE;
            set_status("Delete selected item and all contents?");
        } else if (!filesystem.mounted &&
                   (mogfs_ata_device_init(&block_device))) {
            input_mode = FILE_MANAGER_CONFIRM_FORMAT;
            set_status("Format MogFS region? This erases MogFS files only.");
        }
    } else {
        return;
    }
    draw_manager();
}

#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef enum {
    BT_DEBUG_SD_OFF,
    BT_DEBUG_SD_PENDING,
    BT_DEBUG_SD_NO_CARD,
    BT_DEBUG_SD_RECORDING,
    BT_DEBUG_SD_ERROR,
    BT_DEBUG_SD_LIMIT,
} bt_debug_sd_state_t;

typedef struct {
    FILE *file;
    char directory[160];
    char path[192];
    size_t file_limit;
    unsigned max_files;
    unsigned file_index;
    uint32_t session;
    size_t file_bytes;
    uint64_t bytes_written;
    bt_debug_sd_state_t state;
    int last_errno;
} bt_debug_storage_t;

void bt_debug_storage_init(bt_debug_storage_t *storage, size_t file_limit, unsigned max_files);
bool bt_debug_storage_open(bt_debug_storage_t *storage, bool mounted, const char *mount, uint32_t session);
bool bt_debug_storage_write(bt_debug_storage_t *storage, const char *data, size_t size);
bool bt_debug_storage_flush(bt_debug_storage_t *storage);
void bt_debug_storage_close(bt_debug_storage_t *storage);

/* Project-level adapter over official ESP log, NVS and Board Manager APIs. */
#pragma once
#include "esp_err.h"
#include "bt_debug_policy.h"
#include "bt_debug_storage.h"

typedef struct {
    bool sd_requested;
    bt_debug_sd_state_t sd_state;
    uint32_t dropped;
    uint32_t truncated;
    uint64_t bytes_written;
    int sd_errno;
    esp_err_t settings_error;
    bool settings_pending;
    char path[192];
} bt_debug_status_t;

esp_err_t bt_debug_init(void);
bool bt_debug_supported(size_t index);
bool bt_debug_enabled(size_t index);
bool bt_debug_effective(size_t index);
bool bt_debug_overridden(size_t index);
esp_err_t bt_debug_set(size_t index, bool enabled);
void bt_debug_all_off(void);
void bt_debug_sd_set(bool enabled);
void bt_debug_get_status(bt_debug_status_t *status);
const char *bt_debug_sd_state_name(bt_debug_sd_state_t state);

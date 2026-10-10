/* Project debug policy; independent of LVGL, ESP-IDF and storage. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    BT_DEBUG_CAP_DEBUG = 1u << 0,
    BT_DEBUG_CAP_CLASSIC = 1u << 1,
    BT_DEBUG_CAP_UI = 1u << 2,
    BT_DEBUG_CAP_HFP = 1u << 3,
    BT_DEBUG_CAP_BLE = 1u << 4,
};

typedef struct {
    const char *tag;
    int baseline; /* ESP log level: WARN=2, INFO=3, DEBUG=4. */
} bt_debug_tag_t;

typedef struct {
    const char *key;
    const char *title;
    const char *hint;
    uint32_t required_caps;
    bt_debug_tag_t tags[8];
} bt_debug_module_t;

typedef void (*bt_debug_apply_fn)(const char *tag, int level, void *ctx);
typedef struct {
    uint32_t capabilities;
    uint32_t enabled_mask;
    bt_debug_apply_fn apply;
    void *ctx;
} bt_debug_policy_t;

/* Append new modules; indices are persisted in the versioned NVS namespace. */
size_t bt_debug_module_count(void);
const bt_debug_module_t *bt_debug_module_at(size_t index);
int bt_debug_module_find(const char *key);
bool bt_debug_policy_init(bt_debug_policy_t *policy, uint32_t caps, bt_debug_apply_fn apply, void *ctx);
bool bt_debug_policy_supported(const bt_debug_policy_t *policy, size_t index);
bool bt_debug_policy_enabled(const bt_debug_policy_t *policy, size_t index);
bool bt_debug_policy_set(bt_debug_policy_t *policy, size_t index, bool enabled);
void bt_debug_policy_restore(bt_debug_policy_t *policy, uint32_t mask);

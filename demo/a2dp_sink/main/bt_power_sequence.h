#pragma once
#include <stdbool.h>

typedef enum {
    BT_POWER_STEP_NONE,
    BT_POWER_QUIESCE,
    BT_POWER_PROFILES_STOP,
    BT_POWER_HOST_STOP,
    BT_POWER_AUDIO_RELEASE,
    BT_POWER_CONTROLLER_STOP,
    BT_POWER_CONTROLLER_START,
    BT_POWER_AUDIO_START,
    BT_POWER_ACTIVATE,
} bt_power_step_t;

/* Runs on the power worker, never on the UI/BTC/esp_timer callback task. */
int bt_power_sequence(bool enabled, int (*step)(bt_power_step_t, void *),
                      void *ctx, bt_power_step_t *failed);
const char *bt_power_step_name(bt_power_step_t step);

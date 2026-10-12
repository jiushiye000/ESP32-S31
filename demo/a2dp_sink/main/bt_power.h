#pragma once
#include <stdbool.h>
#include "sdkconfig.h"
#include "esp_err.h"
#include "bt_power_sequence.h"

typedef enum {
    BT_POWER_UNAVAILABLE, BT_POWER_ON, BT_POWER_OFF,
    BT_POWER_STARTING, BT_POWER_STOPPING, BT_POWER_ERROR,
} bt_power_state_t;
typedef struct {
    bt_power_state_t state;
    bt_power_step_t step;
    esp_err_t error;
    bool controller_enabled;
} bt_power_status_t;

/* Only the shipped Bluedroid A2DP Sink + AVRCP roles have a restart adapter. */
#if CONFIG_BT_BLUEDROID_ENABLED && CONFIG_GMF_EXAMPLE_A2DP_SINK && \
    !CONFIG_GMF_EXAMPLE_HFP_HF && !CONFIG_GMF_EXAMPLE_PBAP_PCE && \
    !CONFIG_GMF_EXAMPLE_AUDIO_TECH_LE
#define BT_POWER_SUPPORTED 1
#else
#define BT_POWER_SUPPORTED 0
#endif

void bt_power_get_status(bt_power_status_t *status);
esp_err_t bt_power_request(bool enabled); /* Queued, non-blocking. ERROR accepts OFF only. */
bool bt_power_events_allowed(void);
/* Serialize user commands against teardown; take is non-blocking. */
bool bt_power_command_begin(void);
void bt_power_command_end(void);
static inline const char *bt_power_state_name(bt_power_state_t state)
{
    static const char *names[] = {"UNAVAILABLE", "ON", "OFF", "STARTING", "STOPPING", "ERROR"};
    return (unsigned)state < sizeof(names) / sizeof(*names) ? names[state] : "UNKNOWN";
}
#if BT_POWER_SUPPORTED
#include "esp_bt_audio.h"
/* Call prepare before the FIRST esp_bt_audio_init, then ready after boot succeeds.
 * Config + host_config storage must remain valid for the life of the application. */
esp_err_t bt_power_prepare(esp_bt_audio_config_t *config, unsigned controller_mode,
                           esp_err_t (*activate)(void));
esp_err_t bt_power_ready(void);
#endif

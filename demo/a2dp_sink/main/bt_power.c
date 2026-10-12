#include "bt_power.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "esp_gap_bt_api.h"
#include "esp_timer.h"
#include "stream_proc.h"

static portMUX_TYPE status_mux = portMUX_INITIALIZER_UNLOCKED;
static bt_power_status_t status = {.state = BT_POWER_UNAVAILABLE};
static SemaphoreHandle_t command_lock;

void bt_power_get_status(bt_power_status_t *out)
{
    if (!out) return;
    portENTER_CRITICAL(&status_mux);
    *out = status;
    portEXIT_CRITICAL(&status_mux);
}
bool bt_power_events_allowed(void)
{
    bt_power_status_t s;
    bt_power_get_status(&s);
    return s.state == BT_POWER_ON || s.state == BT_POWER_UNAVAILABLE;
}
bool bt_power_command_begin(void)
{
    if (!command_lock) return true; /* Unsupported roles retain their old behavior. */
    if (xSemaphoreTake(command_lock, 0) != pdTRUE) return false;
    if (!bt_power_events_allowed()) {
        xSemaphoreGive(command_lock);
        return false;
    }
    return true;
}
void bt_power_command_end(void)
{
    if (command_lock) xSemaphoreGive(command_lock);
}

#if BT_POWER_SUPPORTED
#define A2D_UP BIT0
#define A2D_DOWN BIT1
#define CT_UP BIT2
#define CT_DOWN BIT3
#define TG_UP BIT4
#define TG_DOWN BIT5
#define LINK_DOWN BIT6
#define PROFILE_TIMEOUT pdMS_TO_TICKS(5000)

static const char *TAG = "BT_POWER";
static QueueHandle_t requests;
static EventGroupHandle_t profiles;
static SemaphoreHandle_t timer_done;
static esp_timer_handle_t timer_barrier;
static esp_bt_audio_config_t *audio_config;
static unsigned bt_mode;
static esp_err_t (*activate_audio)(void);
static bool audio_live;
static bool a2d_stop_pending, ct_stop_pending, tg_stop_pending;
static esp_a2d_cb_t a2d_callback;
static esp_avrc_ct_cb_t ct_callback;
static esp_avrc_tg_cb_t tg_callback;
static esp_a2d_connection_state_t connection_state = ESP_A2D_CONNECTION_STATE_DISCONNECTED;
static uint8_t peer[6];

/* Observe public profile completions while retaining GMF's registered callbacks.
 * Linker wrapping changes no SDK/vendor source. Register BEFORE async init to
 * close GMF's init-before-callback-registration window. */
static void observe_a2d(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
    portENTER_CRITICAL(&status_mux);
    esp_a2d_cb_t cb = a2d_callback;
    portEXIT_CRITICAL(&status_mux);
    if (cb) cb(event, param);
    if (event == ESP_A2D_CONNECTION_STATE_EVT) {
        portENTER_CRITICAL(&status_mux);
        connection_state = param->conn_stat.state;
        memcpy(peer, param->conn_stat.remote_bda, sizeof(peer));
        portEXIT_CRITICAL(&status_mux);
        if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) xEventGroupSetBits(profiles, LINK_DOWN);
        else xEventGroupClearBits(profiles, LINK_DOWN);
    }
    if (event == ESP_A2D_PROF_STATE_EVT) {
        xEventGroupSetBits(profiles, param->a2d_prof_stat.init_state == ESP_A2D_INIT_SUCCESS ? A2D_UP : A2D_DOWN);
    }
}
static void observe_ct(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param)
{
    portENTER_CRITICAL(&status_mux);
    esp_avrc_ct_cb_t cb = ct_callback;
    portEXIT_CRITICAL(&status_mux);
    if (cb) cb(event, param);
    if (event == ESP_AVRC_CT_PROF_STATE_EVT) {
        if (param->avrc_ct_init_stat.state == ESP_AVRC_INIT_SUCCESS) xEventGroupSetBits(profiles, CT_UP);
        if (param->avrc_ct_init_stat.state == ESP_AVRC_DEINIT_SUCCESS) xEventGroupSetBits(profiles, CT_DOWN);
    }
}
static void observe_tg(esp_avrc_tg_cb_event_t event, esp_avrc_tg_cb_param_t *param)
{
    portENTER_CRITICAL(&status_mux);
    esp_avrc_tg_cb_t cb = tg_callback;
    portEXIT_CRITICAL(&status_mux);
    if (cb) cb(event, param);
    if (event == ESP_AVRC_TG_PROF_STATE_EVT) {
        if (param->avrc_tg_init_stat.state == ESP_AVRC_INIT_SUCCESS) xEventGroupSetBits(profiles, TG_UP);
        if (param->avrc_tg_init_stat.state == ESP_AVRC_DEINIT_SUCCESS) xEventGroupSetBits(profiles, TG_DOWN);
    }
}
esp_err_t __real_esp_a2d_register_callback(esp_a2d_cb_t cb);
esp_err_t __real_esp_avrc_ct_register_callback(esp_avrc_ct_cb_t cb);
esp_err_t __real_esp_avrc_tg_register_callback(esp_avrc_tg_cb_t cb);
esp_err_t __real_esp_a2d_sink_init(void);
esp_err_t __real_esp_avrc_ct_init(void);
esp_err_t __real_esp_avrc_tg_init(void);
esp_err_t __wrap_esp_a2d_register_callback(esp_a2d_cb_t cb)
{
    esp_err_t err = __real_esp_a2d_register_callback(profiles && cb ? observe_a2d : cb);
    if (err != ESP_OK) return err;
    portENTER_CRITICAL(&status_mux);
    a2d_callback = cb;
    portEXIT_CRITICAL(&status_mux);
    return ESP_OK;
}
esp_err_t __wrap_esp_avrc_ct_register_callback(esp_avrc_ct_cb_t cb)
{
    esp_err_t err = __real_esp_avrc_ct_register_callback(profiles && cb ? observe_ct : cb);
    if (err != ESP_OK) return err;
    portENTER_CRITICAL(&status_mux);
    ct_callback = cb;
    portEXIT_CRITICAL(&status_mux);
    return ESP_OK;
}
esp_err_t __wrap_esp_avrc_tg_register_callback(esp_avrc_tg_cb_t cb)
{
    esp_err_t err = __real_esp_avrc_tg_register_callback(profiles && cb ? observe_tg : cb);
    if (err != ESP_OK) return err;
    portENTER_CRITICAL(&status_mux);
    tg_callback = cb;
    portEXIT_CRITICAL(&status_mux);
    return ESP_OK;
}
esp_err_t __wrap_esp_a2d_sink_init(void)
{
    esp_err_t err = profiles ? __real_esp_a2d_register_callback(observe_a2d) : ESP_OK;
    return err == ESP_OK ? __real_esp_a2d_sink_init() : err;
}
esp_err_t __wrap_esp_avrc_ct_init(void)
{
    esp_err_t err = profiles ? __real_esp_avrc_ct_register_callback(observe_ct) : ESP_OK;
    return err == ESP_OK ? __real_esp_avrc_ct_init() : err;
}
esp_err_t __wrap_esp_avrc_tg_init(void)
{
    esp_err_t err = profiles ? __real_esp_avrc_tg_register_callback(observe_tg) : ESP_OK;
    return err == ESP_OK ? __real_esp_avrc_tg_init() : err;
}
static EventBits_t wanted(bool up)
{
    EventBits_t bits = up ? A2D_UP : A2D_DOWN;
    if (audio_config->classic.roles & ESP_BT_AUDIO_CLASSIC_ROLE_AVRC_CT) bits |= up ? CT_UP : CT_DOWN;
    if (audio_config->classic.roles & ESP_BT_AUDIO_CLASSIC_ROLE_AVRC_TG) bits |= up ? TG_UP : TG_DOWN;
    return bits;
}
static esp_err_t wait_profiles(bool up)
{
    EventBits_t bits = wanted(up);
    return (xEventGroupWaitBits(profiles, bits, pdFALSE, pdTRUE, PROFILE_TIMEOUT) & bits) == bits ? ESP_OK : ESP_ERR_TIMEOUT;
}
static void barrier_callback(void *arg)
{
    (void)arg;
    xSemaphoreGive(timer_done);
}
static esp_err_t drain_key_timers(void)
{
    /* Locked GMF v1.0 uses 200ms TASK-dispatch key timers. All command entrants
     * are now gated. A later callback on that SAME serial esp_timer task proves
     * preceding key callbacks returned; sleeping 300ms would not prove this. */
    esp_err_t err = esp_timer_stop_blocking(timer_barrier, PROFILE_TIMEOUT);
    if (err != ESP_OK) return err;
    xSemaphoreTake(timer_done, 0);
    err = esp_timer_start_once(timer_barrier, 300000);
    if (err != ESP_OK) return err;
    return xSemaphoreTake(timer_done, PROFILE_TIMEOUT) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}
static esp_err_t stop_profiles(void)
{
    if (!audio_live) return ESP_OK;
    EventBits_t done = xEventGroupGetBits(profiles);
    if ((done & wanted(false)) == wanted(false)) return ESP_OK;
    if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) return ESP_ERR_INVALID_STATE;
    esp_err_t err;
    uint8_t address[6];
    portENTER_CRITICAL(&status_mux);
    bool linked = connection_state != ESP_A2D_CONNECTION_STATE_DISCONNECTED;
    memcpy(address, peer, sizeof(address));
    portEXIT_CRITICAL(&status_mux);
    if (linked) {
        err = esp_a2d_sink_disconnect(address);
        if (err != ESP_OK) return err;
        if (!(xEventGroupWaitBits(profiles, LINK_DOWN, pdFALSE, pdTRUE, PROFILE_TIMEOUT) & LINK_DOWN)) return ESP_ERR_TIMEOUT;
    }
    /* Official AVRCP contract: deinit before A2DP. The A2DP completion is async,
     * so queued-request success alone must never authorize freeing contexts. */
    if ((wanted(false) & CT_DOWN) && !(done & CT_DOWN) && !ct_stop_pending) {
        err = esp_avrc_ct_deinit();
        if (err != ESP_OK) return err;
        ct_stop_pending = true;
    }
    if ((wanted(false) & TG_DOWN) && !(done & TG_DOWN) && !tg_stop_pending) {
        err = esp_avrc_tg_deinit();
        if (err != ESP_OK) return err;
        tg_stop_pending = true;
    }
    if (!(done & A2D_DOWN) && !a2d_stop_pending) {
        err = esp_a2d_sink_deinit();
        if (err != ESP_OK) return err;
        a2d_stop_pending = true;
    }
    return wait_profiles(false);
}
static int run_step(bt_power_step_t step, void *ctx)
{
    (void)ctx;
    portENTER_CRITICAL(&status_mux);
    status.step = step;
    portEXIT_CRITICAL(&status_mux);
    ESP_LOGI(TAG, "%s", bt_power_step_name(step));
    esp_err_t err;
    switch (step) {
        case BT_POWER_QUIESCE:
            err = stream_proc_quiesce();
            if (err != ESP_OK) return err;
            if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED) {
                err = esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
                if (err != ESP_OK) return err;
            }
            return drain_key_timers();
        case BT_POWER_PROFILES_STOP:
            return stop_profiles();
        case BT_POWER_HOST_STOP:
            if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED) {
                err = esp_bluedroid_disable();
                if (err != ESP_OK) return err;
            }
            /* Destroy the BTC callback task before freeing any GMF context.
             * Profiles were already cleanly deinitialized above. */
            return esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_INITIALIZED ? esp_bluedroid_deinit() : ESP_OK;
        case BT_POWER_AUDIO_RELEASE:
            if (audio_live) {
                /* No BTC/data/key callback or pipeline reader may use GMF now. */
                esp_bt_audio_deinit();
                audio_live = false;
            }
            return esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_UNINITIALIZED ? ESP_OK : ESP_ERR_INVALID_STATE;
        case BT_POWER_CONTROLLER_STOP:
            err = esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED ? esp_bt_controller_disable() : ESP_OK;
            if (err != ESP_OK) return err;
            return esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED ? ESP_OK : ESP_ERR_INVALID_STATE;
        case BT_POWER_CONTROLLER_START:
            return esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED ? ESP_OK : esp_bt_controller_enable(bt_mode);
        case BT_POWER_AUDIO_START:
            if (!audio_live) {
                xEventGroupClearBits(profiles, A2D_UP | A2D_DOWN | CT_UP | CT_DOWN | TG_UP | TG_DOWN);
                a2d_stop_pending = ct_stop_pending = tg_stop_pending = false;
                err = esp_bt_audio_init(audio_config);
                if (err != ESP_OK) return err;
                audio_live = true;
            }
            return wait_profiles(true);
        case BT_POWER_ACTIVATE:
            err = activate_audio();
            if (err == ESP_OK) stream_proc_resume();
            return err;
        default:
            return ESP_ERR_INVALID_ARG;
    }
}
static void power_task(void *arg)
{
    (void)arg;
    bool enabled;
    while (xQueueReceive(requests, &enabled, portMAX_DELAY) == pdTRUE) {
        xSemaphoreTake(command_lock, portMAX_DELAY);
        bt_power_step_t failed;
        esp_err_t err = bt_power_sequence(enabled, run_step, NULL, &failed);
        portENTER_CRITICAL(&status_mux);
        status.state = err == ESP_OK ? (enabled ? BT_POWER_ON : BT_POWER_OFF) : BT_POWER_ERROR;
        status.step = failed;
        status.error = err;
        status.controller_enabled = esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED;
        portEXIT_CRITICAL(&status_mux);
        xSemaphoreGive(command_lock);
        if (err != ESP_OK) ESP_LOGE(TAG, "%s failed: %s; retry OFF before ON", bt_power_step_name(failed), esp_err_to_name(err));
        else ESP_LOGI(TAG, "Bluetooth %s (display/console/log policy stay alive)", enabled ? "ON" : "OFF");
    }
}
esp_err_t bt_power_prepare(esp_bt_audio_config_t *config, unsigned mode, esp_err_t (*activate)(void))
{
    if (!config || !config->host_config || !activate || command_lock) return ESP_ERR_INVALID_ARG;
    command_lock = xSemaphoreCreateMutex();
    profiles = xEventGroupCreate();
    timer_done = xSemaphoreCreateBinary();
    requests = xQueueCreate(1, sizeof(bool));
    if (!command_lock || !profiles || !timer_done || !requests) return ESP_ERR_NO_MEM;
    esp_timer_create_args_t args = {.callback = barrier_callback, .name = "bt_key_barrier"};
    esp_err_t err = esp_timer_create(&args, &timer_barrier);
    if (err != ESP_OK) return err;
    audio_config = config;
    bt_mode = mode;
    activate_audio = activate;
    return ESP_OK;
}
esp_err_t bt_power_ready(void)
{
    if (!profiles || !activate_audio) return ESP_ERR_INVALID_STATE;
    audio_live = true;
    esp_err_t err = wait_profiles(true);
    if (err != ESP_OK) return err;
    if (xTaskCreate(power_task, "bt_power", 6144, NULL, 5, NULL) != pdPASS) return ESP_ERR_NO_MEM;
    portENTER_CRITICAL(&status_mux);
    status.state = BT_POWER_ON;
    status.controller_enabled = true;
    portEXIT_CRITICAL(&status_mux);
    return ESP_OK;
}
esp_err_t bt_power_request(bool enabled)
{
    esp_err_t err = ESP_OK;
    portENTER_CRITICAL(&status_mux);
    bt_power_status_t previous = status;
    bt_power_state_t old = status.state;
    if (old == BT_POWER_UNAVAILABLE) err = ESP_ERR_NOT_SUPPORTED;
    else if (old == BT_POWER_STARTING || old == BT_POWER_STOPPING || (old == BT_POWER_ERROR && enabled)) err = ESP_ERR_INVALID_STATE;
    else if (old != (enabled ? BT_POWER_ON : BT_POWER_OFF)) {
        status.state = enabled ? BT_POWER_STARTING : BT_POWER_STOPPING;
        status.error = ESP_OK;
        status.step = BT_POWER_STEP_NONE;
    }
    portEXIT_CRITICAL(&status_mux);
    if (err != ESP_OK || old == (enabled ? BT_POWER_ON : BT_POWER_OFF)) return err;
    if (xQueueSend(requests, &enabled, 0) == pdTRUE) return ESP_OK;
    portENTER_CRITICAL(&status_mux);
    status = previous;
    portEXIT_CRITICAL(&status_mux);
    return ESP_ERR_NO_MEM;
}
#else
esp_err_t bt_power_request(bool enabled)
{
    (void)enabled;
    return ESP_ERR_NOT_SUPPORTED;
}
#endif

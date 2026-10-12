#include "bt_debug.h"
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_board_manager.h"
#include "dev_fs_fat.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define LOG_QUEUE_LENGTH 64
#define LOG_CHUNK_BYTES 256
#define LOG_FILE_BYTES (4u * 1024u * 1024u)
#define LOG_SESSION_FILES 4

typedef struct {
    uint32_t epoch;
    uint16_t size;
    char data[LOG_CHUNK_BYTES];
} log_chunk_t;

static const char *TAG = "BT_DEBUG";
static bt_debug_policy_t policy;
static SemaphoreHandle_t lock;
static QueueHandle_t log_queue;
static TaskHandle_t writer_task;
static vprintf_like_t serial_printf = vprintf;
static nvs_handle_t settings;
static bool settings_open;
static bt_debug_status_t status;
static uint32_t settings_generation;
static uint32_t sd_generation = 1;
static uint32_t capture_epoch;
static uint32_t dropped;
static uint32_t truncated;

/* No private Bluedroid APIs: retained trace code is filtered by ESP log tags. */
static bool trace_compiled(const char *key)
{
#if CONFIG_BT_CLASSIC_ENABLED
    if (!strcmp(key, "GAP")) return CONFIG_BT_LOG_GAP_TRACE_LEVEL >= 5;
    if (!strcmp(key, "BTM")) return CONFIG_BT_LOG_BTM_TRACE_LEVEL >= 5;
    if (!strcmp(key, "HCI")) return CONFIG_BT_LOG_HCI_TRACE_LEVEL >= 5;
    if (!strcmp(key, "L2CAP")) return CONFIG_BT_LOG_L2CAP_TRACE_LEVEL >= 5;
    if (!strcmp(key, "SDP")) return CONFIG_BT_LOG_SDP_TRACE_LEVEL >= 5;
    if (!strcmp(key, "A2D")) return CONFIG_BT_LOG_A2D_TRACE_LEVEL >= 5;
    if (!strcmp(key, "AVDT")) return CONFIG_BT_LOG_AVDT_TRACE_LEVEL >= 5;
    if (!strcmp(key, "AVCT")) return CONFIG_BT_LOG_AVCT_TRACE_LEVEL >= 5;
    if (!strcmp(key, "AVRC")) return CONFIG_BT_LOG_AVRC_TRACE_LEVEL >= 5;
    if (!strcmp(key, "BAL")) return CONFIG_BT_LOG_APPL_TRACE_LEVEL >= 5;
    if (!strcmp(key, "BTC")) return CONFIG_BT_LOG_BTC_TRACE_LEVEL >= 5;
    if (!strcmp(key, "BTIF")) return CONFIG_BT_LOG_BTIF_TRACE_LEVEL >= 5;
    if (!strcmp(key, "OSI")) return CONFIG_BT_LOG_OSI_TRACE_LEVEL >= 5;
#endif
    return true;
}

static void apply_tag(const char *tag, int level, void *ctx)
{
    (void)ctx;
    esp_log_level_set(tag, (esp_log_level_t)level);
}

bool bt_debug_supported(size_t index)
{
    const bt_debug_module_t *module = bt_debug_module_at(index);
    return lock && module && bt_debug_policy_supported(&policy, index) && trace_compiled(module->key);
}

bool bt_debug_enabled(size_t index)
{
    if (!bt_debug_supported(index)) return false;
    xSemaphoreTake(lock, portMAX_DELAY);
    bool enabled = bt_debug_policy_enabled(&policy, index);
    xSemaphoreGive(lock);
    return enabled;
}

bool bt_debug_effective(size_t index)
{
    if (!bt_debug_supported(index)) return false;
    const bt_debug_module_t *module = bt_debug_module_at(index);
    for (size_t i = 0; module->tags[i].tag; ++i) {
        if (esp_log_level_get(module->tags[i].tag) < ESP_LOG_DEBUG) return false;
    }
    return true;
}

/* Detect partial raw tag overrides too, not just all-tags-at-DEBUG changes. */
bool bt_debug_overridden(size_t index)
{
    if (!bt_debug_supported(index)) return false;
    const bt_debug_module_t *module = bt_debug_module_at(index);
    xSemaphoreTake(lock, portMAX_DELAY);
    bool enabled = bt_debug_policy_enabled(&policy, index);
    bool overridden = false;
    for (size_t i = 0; module->tags[i].tag; ++i) {
        int expected = enabled ? ESP_LOG_DEBUG : module->tags[i].baseline;
        if (esp_log_level_get(module->tags[i].tag) != expected) overridden = true;
    }
    xSemaphoreGive(lock);
    return overridden;
}

esp_err_t bt_debug_set(size_t index, bool enabled)
{
    if (!bt_debug_module_at(index)) return ESP_ERR_INVALID_ARG;
    if (!bt_debug_supported(index)) return ESP_ERR_NOT_SUPPORTED;
    xSemaphoreTake(lock, portMAX_DELAY);
    bt_debug_policy_set(&policy, index, enabled);
    ++settings_generation;
    status.settings_pending = true;
    xSemaphoreGive(lock);
    const bt_debug_module_t *module = bt_debug_module_at(index);
    ESP_LOGI(TAG, "DBG_CFG %s=%s; DEBUG=4, OFF=per-tag INFO/WARN", module->key, enabled ? "ON" : "OFF");
    if (enabled) ESP_LOGD(module->tags[0].tag, "DBG_PROBE module=%s runtime DEBUG active", module->key);
    return ESP_OK;
}

void bt_debug_all_off(void)
{
    if (!lock) return;
    xSemaphoreTake(lock, portMAX_DELAY);
    bt_debug_policy_restore(&policy, 0);
    ++settings_generation;
    status.settings_pending = true;
    xSemaphoreGive(lock);
    ESP_LOGI(TAG, "DBG_CFG all modules OFF; warnings/errors remain enabled");
}

void bt_debug_sd_set(bool enabled)
{
    if (!lock) return;
    xSemaphoreTake(lock, portMAX_DELAY);
    __atomic_store_n(&capture_epoch, 0, __ATOMIC_RELEASE);
    status.sd_requested = enabled;
    status.sd_state = enabled ? BT_DEBUG_SD_PENDING : status.sd_state;
    ++sd_generation;
    ++settings_generation;
    status.settings_pending = true;
    xSemaphoreGive(lock);
    ESP_LOGI(TAG, "SD text logging requested %s; serial output retained", enabled ? "ON" : "OFF");
}

void bt_debug_get_status(bt_debug_status_t *out)
{
    if (!out) return;
    if (!lock) { *out = (bt_debug_status_t){.settings_error = ESP_ERR_INVALID_STATE}; return; }
    xSemaphoreTake(lock, portMAX_DELAY);
    *out = status;
    xSemaphoreGive(lock);
    out->dropped = __atomic_load_n(&dropped, __ATOMIC_RELAXED);
    out->truncated = __atomic_load_n(&truncated, __ATOMIC_RELAXED);
}

const char *bt_debug_sd_state_name(bt_debug_sd_state_t state)
{
    switch (state) {
        case BT_DEBUG_SD_OFF: return "OFF";
        case BT_DEBUG_SD_PENDING: return "PENDING";
        case BT_DEBUG_SD_NO_CARD: return "NO CARD / serial only";
        case BT_DEBUG_SD_RECORDING: return "RECORDING";
        case BT_DEBUG_SD_ERROR: return "IO ERROR / serial only";
        case BT_DEBUG_SD_LIMIT: return "SESSION LIMIT / serial only";
        default: return "UNKNOWN";
    }
}

/* Hook never performs filesystem IO, waits for a queue, or logs recursively. */
static int log_tee(const char *format, va_list args)
{
    /* Sample before serial IO: a slow producer must not join a later session. */
    uint32_t epoch = __atomic_load_n(&capture_epoch, __ATOMIC_ACQUIRE);
    va_list serial_args;
    va_copy(serial_args, args);
    vprintf_like_t serial = __atomic_load_n(&serial_printf, __ATOMIC_ACQUIRE);
    int result = serial(format, serial_args);
    va_end(serial_args);
    if (!epoch || xTaskGetCurrentTaskHandle() == writer_task) return result;
    log_chunk_t chunk = {.epoch = epoch};
    va_list file_args;
    va_copy(file_args, args);
    int n = vsnprintf(chunk.data, sizeof(chunk.data), format, file_args);
    va_end(file_args);
    if (n < 0) { __atomic_fetch_add(&dropped, 1, __ATOMIC_RELAXED); return result; }
    if ((size_t)n >= sizeof(chunk.data)) {
        const char tail[] = " [TRUNCATED]\n";
        memcpy(chunk.data + sizeof(chunk.data) - sizeof(tail), tail, sizeof(tail));
        chunk.size = sizeof(chunk.data) - 1;
        __atomic_fetch_add(&truncated, 1, __ATOMIC_RELAXED);
    } else {
        chunk.size = (uint16_t)n;
    }
    if (xQueueSend(log_queue, &chunk, 0) != pdTRUE) __atomic_fetch_add(&dropped, 1, __ATOMIC_RELAXED);
    return result;
}

static void drain(bt_debug_storage_t *storage, uint32_t epoch)
{
    log_chunk_t chunk;
    for (unsigned i = 0; i < LOG_QUEUE_LENGTH && xQueueReceive(log_queue, &chunk, 0) == pdTRUE; ++i) {
        if (chunk.epoch != epoch || !bt_debug_storage_write(storage, chunk.data, chunk.size)) {
            __atomic_fetch_add(&dropped, 1, __ATOMIC_RELAXED);
        }
    }
}

static void publish_storage(const bt_debug_storage_t *storage, uint32_t generation)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    if (generation == sd_generation) {
        status.sd_state = storage->state;
        status.sd_errno = storage->last_errno;
        status.bytes_written = storage->bytes_written;
        memcpy(status.path, storage->path, sizeof(status.path));
    }
    xSemaphoreGive(lock);
}

static void save_settings(uint32_t generation, uint32_t mask, bool sd_requested)
{
    esp_err_t err = settings_open ? nvs_set_u32(settings, "modules", mask) : ESP_ERR_INVALID_STATE;
    if (err == ESP_OK) err = nvs_set_u8(settings, "sd", sd_requested);
    if (err == ESP_OK) err = nvs_commit(settings);
    xSemaphoreTake(lock, portMAX_DELAY);
    status.settings_error = err;
    if (settings_generation == generation) status.settings_pending = false;
    xSemaphoreGive(lock);
    if (err != ESP_OK) ESP_LOGW(TAG, "Cannot persist debug settings: %s", esp_err_to_name(err));
}

static void writer(void *arg)
{
    (void)arg;
    bt_debug_storage_t storage;
    bt_debug_storage_init(&storage, LOG_FILE_BYTES, LOG_SESSION_FILES);
    uint32_t active_sd = 0, observed_settings = 0, saved_settings = 0;
    TickType_t settings_changed = 0;
    unsigned flush_ticks = 0;
    for (;;) {
        xSemaphoreTake(lock, portMAX_DELAY);
        uint32_t sd_seq = sd_generation, cfg_seq = settings_generation;
        bool requested = status.sd_requested;
        uint32_t mask = policy.enabled_mask;
        xSemaphoreGive(lock);
        if (sd_seq != active_sd) {
            __atomic_store_n(&capture_epoch, 0, __ATOMIC_RELEASE);
            drain(&storage, active_sd);
            bt_debug_storage_close(&storage);
            active_sd = sd_seq;
            if (requested) {
                /* Board Manager owns mounting; query once per enable request. */
                dev_fs_fat_handle_t *sd = NULL;
                esp_err_t err = esp_board_manager_get_device_handle("fs_sdcard", (void **)&sd);
                bool mounted = err == ESP_OK && sd && sd->card && sd->mount_point;
                bt_debug_storage_open(&storage, mounted, mounted ? sd->mount_point : NULL, esp_random());
                if (storage.state == BT_DEBUG_SD_RECORDING) {
                    /* A concurrent OFF/ON must not re-enable a stale recording. */
                    xSemaphoreTake(lock, portMAX_DELAY);
                    if (active_sd == sd_generation && status.sd_requested) {
                        __atomic_store_n(&capture_epoch, active_sd, __ATOMIC_RELEASE);
                    }
                    xSemaphoreGive(lock);
                }
                ESP_LOGI(TAG, "SD text sink: %s, errno=%d", bt_debug_sd_state_name(storage.state), storage.last_errno);
            }
        }
        drain(&storage, active_sd);
        if (++flush_ticks >= 5) {
            if (storage.file) bt_debug_storage_flush(&storage);
            flush_ticks = 0;
        }
        if (storage.state != BT_DEBUG_SD_RECORDING) __atomic_store_n(&capture_epoch, 0, __ATOMIC_RELEASE);
        publish_storage(&storage, active_sd);
        TickType_t now = xTaskGetTickCount();
        if (cfg_seq != observed_settings) { observed_settings = cfg_seq; settings_changed = now; }
        if (cfg_seq != saved_settings && (TickType_t)(now - settings_changed) >= pdMS_TO_TICKS(500)) {
            save_settings(cfg_seq, mask, requested);
            saved_settings = cfg_seq;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

esp_err_t bt_debug_init(void)
{
    if (lock) return ESP_ERR_INVALID_STATE;
    lock = xSemaphoreCreateMutex();
    log_queue = xQueueCreate(LOG_QUEUE_LENGTH, sizeof(log_chunk_t));
    if (!lock || !log_queue) {
        if (lock) vSemaphoreDelete(lock);
        if (log_queue) vQueueDelete(log_queue);
        lock = NULL;
        log_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    uint32_t caps = 0;
#if CONFIG_LOG_MAXIMUM_LEVEL >= 4 && CONFIG_LOG_DYNAMIC_LEVEL_CONTROL
    caps |= BT_DEBUG_CAP_DEBUG;
#endif
#if CONFIG_BT_CLASSIC_ENABLED
    caps |= BT_DEBUG_CAP_CLASSIC;
#endif
#if CONFIG_EXAMPLE_BT_UI_ENABLE
    caps |= BT_DEBUG_CAP_UI;
#endif
#if CONFIG_GMF_EXAMPLE_HFP_HF && CONFIG_BT_HFP_CLIENT_ENABLE && CONFIG_BT_LOG_RFCOMM_TRACE_LEVEL >= 5
    caps |= BT_DEBUG_CAP_HFP;
#endif
#if CONFIG_BT_BLE_ENABLED && CONFIG_BT_LOG_GATT_TRACE_LEVEL >= 5 && CONFIG_BT_LOG_SMP_TRACE_LEVEL >= 5
    caps |= BT_DEBUG_CAP_BLE;
#endif
    bt_debug_policy_init(&policy, caps, apply_tag, NULL);
    status.settings_error = nvs_open("bt_debug_v1", NVS_READWRITE, &settings);
    settings_open = status.settings_error == ESP_OK;
    uint32_t mask = 0;
    uint8_t sd_enabled = 0;
    if (settings_open) {
        esp_err_t err = nvs_get_u32(settings, "modules", &mask);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) status.settings_error = err;
        err = nvs_get_u8(settings, "sd", &sd_enabled);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) status.settings_error = err;
    }
    for (size_t i = 0; i < bt_debug_module_count(); ++i) {
        if (!bt_debug_supported(i)) mask &= ~(1u << i);
    }
    bt_debug_policy_restore(&policy, mask);
    status.sd_requested = sd_enabled != 0;
    status.sd_state = status.sd_requested ? BT_DEBUG_SD_PENDING : BT_DEBUG_SD_OFF;
    if (xTaskCreate(writer, "bt_log_writer", 4096, NULL, 2, &writer_task) != pdPASS) {
        if (settings_open) nvs_close(settings);
        settings_open = false;
        vSemaphoreDelete(lock);
        vQueueDelete(log_queue);
        lock = NULL;
        log_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    vprintf_like_t original = esp_log_set_vprintf(log_tee);
    __atomic_store_n(&serial_printf, original, __ATOMIC_RELEASE);
    ESP_LOGI(TAG, "Debug registry ready: %u modules; mask=0x%08" PRIx32 "; no profile roles changed",
             (unsigned)bt_debug_module_count(), mask);
    return ESP_OK;
}

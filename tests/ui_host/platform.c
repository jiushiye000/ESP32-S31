#include <setjmp.h>
#include <string.h>
#include "platform.h"
#include "jpeg_decoder.h"
#include "bt_debug.h"
#include "bt_ui.h"
struct host_queue { unsigned length, size, count; unsigned char data[128]; };
unsigned host_pending_covers;
bool host_decode_fail, host_decode_locked;
unsigned host_decode_calls;
void *host_during_decode;
lvgl_port_display_cfg_t host_display;
lvgl_port_display_rgb_cfg_t host_rgb;
static void (*cover_task)(void *);
static void *cover_arg;
static jmp_buf task_exit;
static int locked;
QueueHandle_t xQueueCreate(unsigned length, unsigned size)
{
    assert(length * size <= 128);
    struct host_queue *q = calloc(1, sizeof(*q));
    q->length = length; q->size = size; return q;
}
int xQueueSend(QueueHandle_t q, const void *data, unsigned ticks)
{
    (void)ticks;
    if (q->count == q->length) return 0;
    memcpy(q->data + q->size * q->count++, data, q->size);
    host_pending_covers = q->count; return pdTRUE;
}
int xQueueReceive(QueueHandle_t q, void *data, unsigned ticks)
{
    if (!q->count) { if (ticks == portMAX_DELAY) longjmp(task_exit, 1); return 0; }
    memcpy(data, q->data, q->size);
    memmove(q->data, q->data + q->size, --q->count * q->size);
    host_pending_covers = q->count; return pdTRUE;
}
void vQueueDelete(QueueHandle_t q) { free(q); }
int xTaskCreate(void (*task)(void *), const char *name, unsigned stack, void *arg, unsigned prio, void *handle)
{
    (void)name; (void)stack; (void)prio; (void)handle; cover_task = task; cover_arg = arg; return pdPASS;
}
void host_run_cover_task(void) { if (!setjmp(task_exit)) cover_task(cover_arg); assert(!locked); }
bool lvgl_port_lock(unsigned timeout) { (void)timeout; ++locked; return true; }
void lvgl_port_unlock(void) { assert(locked > 0); --locked; }
void *heap_caps_malloc_prefer(size_t size, size_t count, ...) { (void)count; return malloc(size); }
void *heap_caps_malloc(size_t size, unsigned caps) { (void)caps; return malloc(size); }
const char *esp_err_to_name(int err) { (void)err; return "host error"; }
int64_t esp_timer_get_time(void) { return 1; }
esp_err_t esp_bt_audio_stream_get_codec_info(void *s, esp_bt_audio_stream_codec_info_t *i) { (void)s; (void)i; return ESP_FAIL; }
esp_err_t esp_bt_audio_stream_get_profile(void *s, esp_bt_audio_stream_profile_t *p) { (void)s; (void)p; return ESP_FAIL; }
/* Fake JPEG data isolates widget ownership/locking; this is not a decoder test. */
#ifndef UI_HOST_REAL_JPEG
esp_err_t esp_jpeg_get_image_info(esp_jpeg_image_cfg_t *cfg, esp_jpeg_image_output_t *out)
{
    if (host_decode_fail || !cfg->indata || !cfg->indata_size) return ESP_FAIL;
    *out = (esp_jpeg_image_output_t){.width = 32, .height = 32, .output_len = 2048}; return ESP_OK;
}
esp_err_t esp_jpeg_decode(esp_jpeg_image_cfg_t *cfg, esp_jpeg_image_output_t *out)
{
    ++host_decode_calls;
    host_decode_locked = locked != 0;
    if (host_during_decode) { bt_ui_post_cover(host_during_decode, NULL, 0); host_during_decode = NULL; }
    if (host_decode_fail) return ESP_FAIL;
    memset(cfg->outbuf, 0x7f, 2048);
    *out = (esp_jpeg_image_output_t){.width = 32, .height = 32, .output_len = 2048}; return ESP_OK;
}
#endif
esp_err_t esp_board_manager_get_device_handle(const char *name, void **handle)
{
    if (strcmp(name, ESP_BOARD_DEVICE_NAME_DISPLAY_LCD)) return ESP_FAIL;
    static dev_display_lcd_handles_t lcd; *handle = &lcd; return ESP_OK;
}
esp_err_t esp_board_manager_get_device_config(const char *name, void **config)
{
    (void)name; static dev_display_lcd_config_t lcd = {.sub_type = "rgb", .lcd_width = 800, .lcd_height = 480,
        .sub_cfg.rgb.panel_config = {.num_fbs = 2}}; *config = &lcd; return ESP_OK;
}
esp_err_t lvgl_port_init(const lvgl_port_cfg_t *cfg) { (void)cfg; return ESP_OK; }
lv_display_t *lvgl_port_add_disp(const lvgl_port_display_cfg_t *cfg) { host_display = *cfg; return lv_display_get_default(); }
lv_display_t *lvgl_port_add_disp_rgb(const lvgl_port_display_cfg_t *cfg, const lvgl_port_display_rgb_cfg_t *rgb)
{
    host_rgb = *rgb; return lvgl_port_add_disp(cfg);
}
void *lvgl_port_add_touch(const lvgl_port_touch_cfg_t *cfg) { (void)cfg; return NULL; }
bool bt_debug_supported(size_t i) { return i < 17; }
static bool debug_enabled[32];
static bool debug_sd_requested;
bool bt_debug_enabled(size_t i) { assert(i < 32); return debug_enabled[i]; }
bool bt_debug_effective(size_t i) { return bt_debug_enabled(i); }
bool bt_debug_overridden(size_t i) { (void)i; return false; }
esp_err_t bt_debug_set(size_t i, bool on) { assert(i < 32); debug_enabled[i] = on; return ESP_OK; }
void bt_debug_all_off(void) { memset(debug_enabled, 0, sizeof(debug_enabled)); }
void bt_debug_sd_set(bool on) { debug_sd_requested = on; }
void bt_debug_get_status(bt_debug_status_t *s) { *s = (bt_debug_status_t){.sd_requested = debug_sd_requested}; }
const char *bt_debug_sd_state_name(bt_debug_sd_state_t s) { (void)s; return "OFF"; }

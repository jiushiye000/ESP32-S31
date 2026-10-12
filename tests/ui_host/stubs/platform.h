/* Host-only platform stand-ins. Widgets/styles/fonts use the real locked LVGL. */
#pragma once
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "lvgl.h"
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_NOT_SUPPORTED 0x106
#define ESP_ERROR_CHECK(x) assert((x) == ESP_OK)
#define ESP_RETURN_ON_ERROR(x, tag, ...) do { int err = (x); if (err) return err; } while (0)
#define ESP_RETURN_ON_FALSE(x, err, tag, ...) do { if (!(x)) return (err); } while (0)
#define ESP_GOTO_ON_FALSE(x, code, label, tag, ...) do { if (!(x)) { ret = (code); goto label; } } while (0)
#define ESP_LOGD(tag, ...) ((void)(tag))
#define ESP_LOGI(tag, ...) ((void)(tag))
#define ESP_LOGW(tag, ...) ((void)(tag))
#define ESP_LOGE(tag, ...) ((void)(tag))
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define MALLOC_CAP_DEFAULT 4
void *heap_caps_malloc_prefer(size_t size, size_t count, ...);
void *heap_caps_malloc(size_t size, unsigned caps);
const char *esp_err_to_name(int err);
int64_t esp_timer_get_time(void);
typedef int BaseType_t;
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
#define portMAX_DELAY UINT32_MAX
#define pdTRUE 1
#define pdPASS 1
typedef struct host_queue *QueueHandle_t;
QueueHandle_t xQueueCreate(unsigned length, unsigned size);
int xQueueSend(QueueHandle_t queue, const void *data, unsigned ticks);
int xQueueReceive(QueueHandle_t queue, void *data, unsigned ticks);
void vQueueDelete(QueueHandle_t queue);
int xTaskCreate(void (*task)(void *), const char *name, unsigned stack, void *arg, unsigned prio, void *handle);
typedef void *esp_bt_audio_stream_handle_t;
typedef struct { void *codec_cfg; } esp_bt_audio_stream_codec_info_t;
typedef enum { ESP_BT_AUDIO_STREAM_STATE_ALLOCATED, ESP_BT_AUDIO_STREAM_STATE_STARTED,
    ESP_BT_AUDIO_STREAM_STATE_STOPPED, ESP_BT_AUDIO_STREAM_STATE_RELEASED } esp_bt_audio_stream_state_t;
typedef enum { ESP_BT_AUDIO_STREAM_PROFILE_UNKNOWN, ESP_BT_AUDIO_STREAM_PROFILE_LE_UNICAST,
    ESP_BT_AUDIO_STREAM_PROFILE_LE_BROADCAST } esp_bt_audio_stream_profile_t;
enum { ESP_BT_AUDIO_PLAYBACK_STATUS_STOPPED, ESP_BT_AUDIO_PLAYBACK_STATUS_PLAYING,
    ESP_BT_AUDIO_PLAYBACK_STATUS_PAUSED, ESP_BT_AUDIO_PLAYBACK_STATUS_FWD_SEEK,
    ESP_BT_AUDIO_PLAYBACK_STATUS_REV_SEEK, ESP_BT_AUDIO_PLAYBACK_STATUS_ERROR };
enum { ESP_BT_AUDIO_CALL_STATE_INCOMING };
esp_err_t esp_bt_audio_stream_get_codec_info(void *stream, esp_bt_audio_stream_codec_info_t *info);
esp_err_t esp_bt_audio_stream_get_profile(void *stream, esp_bt_audio_stream_profile_t *profile);
typedef struct { int unused; } lvgl_port_cfg_t;
#define ESP_LVGL_PORT_INIT_CONFIG() {0}
typedef struct {
    void *io_handle, *panel_handle;
    size_t buffer_size;
    bool double_buffer, monochrome;
    int hres, vres, color_format;
    struct { bool swap_xy, mirror_x, mirror_y; } rotation;
    struct { bool buff_dma, buff_spiram, direct_mode, full_refresh, sw_rotate; } flags;
} lvgl_port_display_cfg_t;
typedef struct { struct { bool bb_mode, avoid_tearing; } flags; } lvgl_port_display_rgb_cfg_t;
typedef struct { void *disp, *handle; } lvgl_port_touch_cfg_t;
bool lvgl_port_lock(unsigned timeout);
void lvgl_port_unlock(void);
esp_err_t lvgl_port_init(const lvgl_port_cfg_t *cfg);
lv_display_t *lvgl_port_add_disp(const lvgl_port_display_cfg_t *cfg);
lv_display_t *lvgl_port_add_disp_rgb(const lvgl_port_display_cfg_t *cfg, const lvgl_port_display_rgb_cfg_t *rgb);
void *lvgl_port_add_touch(const lvgl_port_touch_cfg_t *cfg);
typedef struct { void *io_handle, *panel_handle; } dev_display_lcd_handles_t;
typedef struct {
    const char *sub_type;
    int lcd_width, lcd_height;
    bool swap_xy, mirror_x, mirror_y;
    struct { struct { struct { int bounce_buffer_size_px, num_fbs; } panel_config; } rgb; } sub_cfg;
} dev_display_lcd_config_t;
typedef struct { void *touch_handle; } dev_lcd_touch_handles_t;
#define ESP_BOARD_DEVICE_NAME_DISPLAY_LCD "display_lcd"
#define ESP_BOARD_DEVICE_NAME_LCD_TOUCH "lcd_touch"
esp_err_t esp_board_manager_get_device_handle(const char *name, void **handle);
esp_err_t esp_board_manager_get_device_config(const char *name, void **config);
void host_run_cover_task(void);
extern unsigned host_pending_covers;
extern bool host_decode_fail;
extern bool host_decode_locked;
extern unsigned host_decode_calls;
extern void *host_during_decode;
extern lvgl_port_display_cfg_t host_display;
extern lvgl_port_display_rgb_cfg_t host_rgb;

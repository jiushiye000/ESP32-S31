/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Private to UI translation units; applications include bt_ui.h only. */
#ifndef BT_UI_INTERNAL_H
#define BT_UI_INTERNAL_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "bt_ui.h"
#include "bt_debug.h"
#include "esp_bt_audio_defs.h"
#include "esp_bt_audio_playback.h"
#include "esp_bt_audio_tel.h"
#include "codec_defs.h"

#define UI_TAB_BAR_HEIGHT 44
#define CONTENT_H (BT_UI_HEIGHT - UI_TAB_BAR_HEIGHT)
#define COVER_IMG_SIZE 340

const lv_font_t *bt_ui_text_font(void);
#define BT_UI_FONT_LARGE (bt_ui_text_font())
#define BT_UI_FONT_MEDIUM (bt_ui_text_font())
#define BT_UI_FONT_SMALL (&lv_font_montserrat_20)
#define BT_UI_FONT_ICON (&lv_font_montserrat_28)
#define BT_UI_FONT_COVER_ICON (&lv_font_montserrat_32)

/**
 * @brief  Cover image payload queued to the UI task.
 */
typedef struct {
    uint8_t  *data;  /*!< Encoded image buffer owned by the UI task */
    size_t    size;  /*!< Encoded image buffer size in bytes */
    uint32_t  generation; /*!< App queue generation, NOT the remote track UID */
} ui_cover_msg_t;

typedef enum {
    UI_DEBUG_CONNECTION,
    UI_DEBUG_STREAM,
    UI_DEBUG_PLAYBACK,
    UI_DEBUG_VOLUME,
} ui_debug_event_type_t;

/* Value-only snapshots: never queue borrowed Bluetooth pointers. */
typedef struct {
    ui_debug_event_type_t             type;
    int64_t                          t_us;
    uint8_t                          addr[6];
    bool                             connected;
    uint32_t                         value;
    bool                             format_valid;
    esp_bt_audio_stream_codec_info_t  format;
} ui_debug_event_t;

typedef struct {
    bool                             connected;
    uint8_t                          addr[6];
    uint32_t                         stream_state;
    uint32_t                         playback_state;
    int                              volume;
    esp_bt_audio_stream_codec_info_t  format;
    bool                             format_valid;
} ui_debug_snapshot_t;

/**
 * @brief  Runtime state for the Bluetooth demo UI.
 */
struct bt_ui_t {
    lv_disp_t                    *disp;                      /*!< Display instance used by LVGL */
    lv_obj_t                     *splash;                    /*!< Splash screen root object */
    lv_obj_t                     *main;                      /*!< Main UI root object */
    lv_obj_t                     *tabview;                   /*!< Main tabview (tab 0 = dialer, tab 1 = media) */
    lv_obj_t                     *media;                     /*!< Media page root object */
    lv_obj_t                     *dialer;                    /*!< Dialer page root object */
    lv_obj_t                     *volume_bar;                /*!< Floating volume indicator */
    int                           volume;                    /*!< Tracked volume level */
    esp_bt_audio_stream_handle_t  stream_type_stream;        /*!< Stream used to derive media placeholder state */
    bool                          stream_type_is_broadcast;  /*!< True when stream_type_stream is a BIS stream */
    QueueHandle_t                 cover_queue;               /*!< Queue carrying cover image updates */
    portMUX_TYPE                  cover_mux;
    uint32_t                      cover_generation;
    lv_obj_t                     *debug_status;
    lv_obj_t                     *debug_switches[32];
    lv_obj_t                     *debug_levels[32];
    lv_obj_t                     *debug_sd_switch;
    lv_obj_t                     *power_switch;
    lv_obj_t                     *power_status;
    lv_obj_t                     *power_retry;
    bt_ui_config_t                power_config;
    bt_power_state_t              last_power_state;
    esp_err_t                     power_request_error;
    uint32_t                      media_tab_index;
    bool                          connected;
    portMUX_TYPE                  debug_mux;
    ui_debug_snapshot_t           debug_snapshot;
};

/**
 * @brief  Cached LVGL widgets and callback state used by the media page.
 */
typedef struct {
    lv_obj_t *play_btn;               /*!< Play/pause button */
    lv_obj_t *title_label;            /*!< Track title label */
    lv_obj_t *artist_label;           /*!< Track artist label */
    lv_obj_t *cover_cont;             /*!< Cover-art container */
    lv_obj_t *cover_img;              /*!< Decoded cover-art image */
    lv_obj_t *cover_placeholder;      /*!< Placeholder shown when no cover art is available */
    lv_obj_t *cover_type_label;       /*!< Fallback text/icon label for stream type */
    lv_obj_t *cover_type_desc_label;  /*!< Broadcast location description label */
    lv_obj_t *cover_type_img;         /*!< Stream-type image label */
    uint8_t  *cover_data;             /*!< Decoded cover-art buffer */
    size_t    cover_size;             /*!< Decoded cover-art buffer size */
#if LVGL_VERSION_MAJOR >= 9
    lv_image_dsc_t  cover_dsc;      /*!< Persistent descriptor for LVGL (src points here) */
#else
    lv_img_dsc_t  cover_img_dsc;   /*!< Persistent descriptor for LVGL (src points here) */
#endif  /* LVGL_VERSION_MAJOR >= 9 */
    esp_err_t (*play_pause_cb)(bool want_play, void *ctx); /*!< Play/pause callback */
    void *play_pause_ctx;                              /*!< Context passed to play_pause_cb */
    void (*prev_cb)(void *ctx);                        /*!< Previous-track callback */
    void (*next_cb)(void *ctx);                        /*!< Next-track callback */
    void *prev_next_ctx;                               /*!< Context passed to prev_cb and next_cb */
#if CONFIG_GMF_EXAMPLE_AUDIO_TECH_LE
    lv_obj_t *speaker_img;          /*!< Speaker-location illustration */
#endif  /* CONFIG_GMF_EXAMPLE_AUDIO_TECH_LE */
} bt_ui_media_refs_t;

/* All page creation/refresh functions run in the existing LVGL context. */
void bt_ui_debug_post(bt_ui_t *ui, ui_debug_event_t *event);
void bt_ui_debug_create(bt_ui_t *ui, lv_obj_t *parent, const char *device_name);
void bt_ui_debug_refresh(bt_ui_t *ui);
void bt_ui_power_create(bt_ui_t *ui, lv_obj_t *parent, const bt_ui_config_t *config);
void bt_ui_power_refresh(bt_ui_t *ui);
/* Queue worker owns encoded data and decodes outside the LVGL lock. */
void bt_ui_cover_task(void *arg);

static inline void bt_ui_switch_state(lv_obj_t *widget, bool checked)
{
    if (checked) lv_obj_add_state(widget, LV_STATE_CHECKED);
    else lv_obj_remove_state(widget, LV_STATE_CHECKED);
}

#endif /* BT_UI_INTERNAL_H */

/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Composition and application update adapters; page widgets live separately. */
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "bt_ui_internal.h"

#define UI_COVER_QUEUE_SIZE 2
#define UI_COVER_TASK_STACK_SIZE 4096
#define UI_COVER_TASK_PRIO 5
static const char *TAG = "BT_UI";

LV_FONT_DECLARE(lv_font_notosanssc_regular_28)
LV_FONT_DECLARE(lv_font_notosansjk_regular_28)
static lv_font_t s_media_font;

static void ui_font_init(void)
{
    s_media_font = lv_font_notosanssc_regular_28;
    s_media_font.fallback = &lv_font_notosansjk_regular_28;
}

const lv_font_t *bt_ui_text_font(void)
{
    return &s_media_font;
}

/* Preserve one shared 500 ms refresh, owned by composition, not by a page. */
static void ui_status_refresh(lv_timer_t *timer)
{
    bt_ui_t *ui = lv_timer_get_user_data(timer);
    if (ui->power_switch) bt_ui_power_refresh(ui);
    bt_ui_debug_refresh(ui);
}

bt_ui_t *bt_ui_create(const char *device_name, const bt_ui_config_t *config)
{
    bt_ui_t *ui = calloc(1, sizeof(bt_ui_t));
    if (ui == NULL) {
        ESP_LOGE(TAG, "Failed to allocate UI handle");
        return NULL;
    }
    ui->volume = 50;
    ui->debug_mux = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;
    ui->cover_mux = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;
    ui->debug_snapshot.volume = 50;
    ui->debug_snapshot.stream_state = UINT32_MAX;
    ui->debug_snapshot.playback_state = UINT32_MAX;
    /* Cover-art queue */
    ui->cover_queue = xQueueCreate(UI_COVER_QUEUE_SIZE, sizeof(ui_cover_msg_t));
    if (ui->cover_queue == NULL) {
        ESP_LOGW(TAG, "Create UI cover queue failed");
        free(ui);
        return NULL;
    }
    BaseType_t task_ret = xTaskCreate(bt_ui_cover_task, "ui_cover", UI_COVER_TASK_STACK_SIZE,
                                      ui, UI_COVER_TASK_PRIO, NULL);
    if (task_ret != pdPASS) {
        vQueueDelete(ui->cover_queue);
        free(ui);
        ESP_LOGW(TAG, "Create UI cover task failed");
        return NULL;
    }

    lvgl_port_lock(0);
    ui_font_init();
    lv_obj_t *scr = lv_scr_act();
    ui->splash = bt_ui_splash_create(scr, device_name);
    lv_obj_add_flag(ui->splash, LV_OBJ_FLAG_HIDDEN);

    ui->main = lv_obj_create(scr);
    lv_obj_remove_style_all(ui->main);
    lv_obj_set_size(ui->main, BT_UI_WIDTH, BT_UI_HEIGHT);
    lv_obj_align(ui->main, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(ui->main, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(ui->main, LV_OPA_TRANSP, 0);

    ui->tabview = lv_tabview_create(ui->main);
    lv_tabview_set_tab_bar_size(ui->tabview, UI_TAB_BAR_HEIGHT);
    lv_obj_set_style_text_font(lv_tabview_get_tab_bar(ui->tabview), BT_UI_FONT_SMALL, 0);
    lv_obj_set_pos(ui->tabview, 0, 0);
    lv_obj_set_size(ui->tabview, BT_UI_WIDTH, BT_UI_HEIGHT);

    ui->media_tab_index = 0;
#if CONFIG_GMF_EXAMPLE_HFP_HF || CONFIG_GMF_EXAMPLE_LE_TMAP_ROLE_CT
    lv_obj_t *tab_dialer = lv_tabview_add_tab(ui->tabview, "Phone");
    lv_obj_clear_flag(tab_dialer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(tab_dialer, 0, 0);
    ui->media_tab_index = 1;
    ui->dialer = bt_ui_dialer_create(tab_dialer,
                                     config ? config->dial_cb : NULL, config ? config->dial_ctx : NULL,
                                     config ? config->end_call_cb : NULL, config ? config->end_call_ctx : NULL,
                                     config ? config->answer_call_cb : NULL, config ? config->answer_call_ctx : NULL);
#endif
    lv_obj_t *tab_music = lv_tabview_add_tab(ui->tabview, "Media");
    lv_obj_clear_flag(tab_music, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(tab_music, 0, 0);
    ui->media = bt_ui_media_create(tab_music,
                                   config ? config->play_pause_cb : NULL, config ? config->play_pause_ctx : NULL,
                                   config ? config->prev_cb : NULL, config ? config->next_cb : NULL,
                                   config ? config->prev_next_ctx : NULL);
    lv_obj_t *tab_debug = lv_tabview_add_tab(ui->tabview, "Debug");
    lv_obj_set_style_pad_all(tab_debug, 0, 0);
    bt_ui_debug_create(ui, tab_debug, device_name);
    lv_timer_t *timer = lv_timer_create(ui_status_refresh, 500, ui);
    ESP_ERROR_CHECK(timer != NULL ? ESP_OK : ESP_ERR_NO_MEM);
    ui_status_refresh(timer);
    lv_obj_t *tab_system = lv_tabview_add_tab(ui->tabview, "System");
    lv_obj_set_style_pad_all(tab_system, 0, 0);
    bt_ui_power_create(ui, tab_system, config);
    ui->volume_bar = bt_ui_volume_bar_create(ui->main);
    bt_ui_volume_bar_set_level(ui->volume_bar, ui->volume);
    lv_tabview_set_act(ui->tabview, ui->media_tab_index + 1, LV_ANIM_OFF);
    lvgl_port_unlock();

    return ui;
}

void bt_ui_set_connected(bt_ui_t *ui, bool connected, const uint8_t addr[6])
{
    if (ui == NULL || addr == NULL) {
        return;
    }
    ui_debug_event_t event = {.type = UI_DEBUG_CONNECTION, .connected = connected};
    memcpy(event.addr, addr, sizeof(event.addr));
    bt_ui_debug_post(ui, &event);
    if (!connected) {
        bt_ui_post_cover(ui, NULL, 0);
        bt_ui_update_track(ui, "", "");
        bt_ui_update_playback_status(ui, ESP_BT_AUDIO_PLAYBACK_STATUS_STOPPED);
    }
}

void bt_ui_update_volume(bt_ui_t *ui, int volume)
{
    if (ui == NULL) {
        return;
    }
    ui->volume = volume < 0 ? 0 : (volume > 100 ? 100 : volume);
    ui_debug_event_t event = {.type = UI_DEBUG_VOLUME, .value = (uint32_t)ui->volume};
    bt_ui_debug_post(ui, &event);
    if (ui->volume_bar == NULL) {
        return;
    }
    lvgl_port_lock(0);
    bt_ui_volume_bar_set_level(ui->volume_bar, ui->volume);
    lvgl_port_unlock();
}

int bt_ui_get_volume(const bt_ui_t *ui)
{
    return ui ? ui->volume : 0;
}

void bt_ui_update_playback_status(bt_ui_t *ui, uint32_t play_status)
{
    if (ui == NULL || ui->media == NULL) {
        return;
    }
    ui_debug_event_t event = {.type = UI_DEBUG_PLAYBACK, .value = play_status};
    bt_ui_debug_post(ui, &event);
    lvgl_port_lock(0);
    bt_ui_media_set_playing(ui->media, play_status == ESP_BT_AUDIO_PLAYBACK_STATUS_PLAYING);
    lvgl_port_unlock();
}

void bt_ui_update_track(bt_ui_t *ui, const char *title, const char *artist)
{
    if (ui == NULL || ui->media == NULL) {
        return;
    }
    lvgl_port_lock(0);
    bt_ui_media_set_track(ui->media, title, artist);
    lvgl_port_unlock();
}

static bt_ui_media_stream_type_t stream_type_from_profile(esp_bt_audio_stream_profile_t profile)
{
    switch (profile) {
        case ESP_BT_AUDIO_STREAM_PROFILE_LE_UNICAST:
            return BT_UI_MEDIA_STREAM_TYPE_CIS;
        case ESP_BT_AUDIO_STREAM_PROFILE_LE_BROADCAST:
            return BT_UI_MEDIA_STREAM_TYPE_BIS;
        default:
            return BT_UI_MEDIA_STREAM_TYPE_UNKNOWN;
    }
}

void bt_ui_update_stream_type(bt_ui_t *ui, esp_bt_audio_stream_handle_t stream,
                              esp_bt_audio_stream_state_t state)
{
    if (ui == NULL || ui->media == NULL || stream == NULL) {
        return;
    }
    ui_debug_event_t event = {.type = UI_DEBUG_STREAM, .value = (uint32_t)state};
    if (state == ESP_BT_AUDIO_STREAM_STATE_ALLOCATED || state == ESP_BT_AUDIO_STREAM_STATE_STARTED) {
        event.format_valid = esp_bt_audio_stream_get_codec_info(stream, &event.format) == ESP_OK;
        event.format.codec_cfg = NULL;
    }
    bt_ui_debug_post(ui, &event);
    if (state == ESP_BT_AUDIO_STREAM_STATE_STARTED) {
        esp_bt_audio_stream_profile_t profile = ESP_BT_AUDIO_STREAM_PROFILE_UNKNOWN;
        if (esp_bt_audio_stream_get_profile(stream, &profile) != ESP_OK) {
            return;
        }
        bt_ui_media_stream_type_t type = stream_type_from_profile(profile);
        if (type == BT_UI_MEDIA_STREAM_TYPE_UNKNOWN) {
            return;
        }
        ui->stream_type_stream = stream;
        ui->stream_type_is_broadcast = (type == BT_UI_MEDIA_STREAM_TYPE_BIS);
        lvgl_port_lock(0);
        bt_ui_media_set_stream_type(ui->media, type);
        if (ui->stream_type_is_broadcast) {
            bt_ui_media_set_track(ui->media, "正在收听广播", "");
        }
        lvgl_port_unlock();
    } else if ((state == ESP_BT_AUDIO_STREAM_STATE_STOPPED || state == ESP_BT_AUDIO_STREAM_STATE_RELEASED) &&
               ui->stream_type_stream == stream) {
        bool clear_broadcast_title = ui->stream_type_is_broadcast;
        ui->stream_type_stream = NULL;
        ui->stream_type_is_broadcast = false;
        lvgl_port_lock(0);
        bt_ui_media_set_stream_type(ui->media, BT_UI_MEDIA_STREAM_TYPE_UNKNOWN);
        if (clear_broadcast_title) {
            bt_ui_media_set_track(ui->media, "", "");
        }
        lvgl_port_unlock();
    }
}

void bt_ui_update_call_state(bt_ui_t *ui, int state, const char *number)
{
    if (ui == NULL || ui->dialer == NULL) {
        return;
    }
    lvgl_port_lock(0);
    bt_ui_dialer_set_call_state(ui->dialer, state, number);
    /* Switch to the dialer tab on any call activity; return to media when idle */
    if (ui->tabview != NULL) {
        lv_tabview_set_act(ui->tabview, state != 0 ? 0 : ui->media_tab_index, LV_ANIM_ON);
    }
    lvgl_port_unlock();
}

lv_obj_t *bt_ui_splash_create(lv_obj_t *parent, const char *device_name)
{
    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, BT_UI_WIDTH, BT_UI_HEIGHT);
    lv_obj_center(root);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x102840), 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *label = lv_label_create(root);
    lv_obj_set_style_text_color(label, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(label, BT_UI_FONT_LARGE, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text_fmt(label, "Device Name:\n%s", device_name && device_name[0] ? device_name : "(BT)");
    lv_obj_center(label);
    return root;
}

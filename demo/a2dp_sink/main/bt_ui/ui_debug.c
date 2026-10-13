/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "bt_ui_internal.h"

static const char *TAG = "BT_UI";

static const char *debug_stream_state(uint32_t state)
{
    switch (state) {
        case ESP_BT_AUDIO_STREAM_STATE_ALLOCATED: return "ALLOCATED";
        case ESP_BT_AUDIO_STREAM_STATE_STARTED: return "STARTED";
        case ESP_BT_AUDIO_STREAM_STATE_STOPPED: return "STOPPED";
        case ESP_BT_AUDIO_STREAM_STATE_RELEASED: return "RELEASED";
        default: return "UNKNOWN";
    }
}

static const char *debug_playback_state(uint32_t state)
{
    switch (state) {
        case ESP_BT_AUDIO_PLAYBACK_STATUS_STOPPED: return "STOPPED";
        case ESP_BT_AUDIO_PLAYBACK_STATUS_PLAYING: return "PLAYING";
        case ESP_BT_AUDIO_PLAYBACK_STATUS_PAUSED: return "PAUSED";
        case ESP_BT_AUDIO_PLAYBACK_STATUS_FWD_SEEK: return "FWD_SEEK";
        case ESP_BT_AUDIO_PLAYBACK_STATUS_REV_SEEK: return "REV_SEEK";
        case ESP_BT_AUDIO_PLAYBACK_STATUS_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

void bt_ui_debug_post(bt_ui_t *ui, ui_debug_event_t *event)
{
    if (ui == NULL) {
        return;
    }
    event->t_us = esp_timer_get_time();
    /* Bluetooth callbacks update only a short, independent state snapshot. */
    portENTER_CRITICAL(&ui->debug_mux);
    ui_debug_snapshot_t *snapshot = &ui->debug_snapshot;
    switch (event->type) {
        case UI_DEBUG_CONNECTION:
            snapshot->connected = event->connected;
            memcpy(snapshot->addr, event->addr, sizeof(snapshot->addr));
            if (!event->connected) {
                snapshot->format_valid = false;
                snapshot->stream_state = UINT32_MAX;
                snapshot->playback_state = UINT32_MAX;
            }
            break;
        case UI_DEBUG_STREAM:
            snapshot->stream_state = event->value;
            if (event->format_valid) {
                snapshot->format = event->format;
                snapshot->format_valid = true;
            } else if (event->value == ESP_BT_AUDIO_STREAM_STATE_RELEASED) {
                snapshot->format_valid = false;
            }
            break;
        case UI_DEBUG_PLAYBACK:
            snapshot->playback_state = event->value;
            break;
        case UI_DEBUG_VOLUME:
            snapshot->volume = (int)event->value;
            break;
    }
    portEXIT_CRITICAL(&ui->debug_mux);
    ESP_LOGD(TAG, "UI_EVENT t_us=%" PRId64 " type=%u value=%" PRIu32,
             event->t_us, (unsigned)event->type, event->value);
}

static void debug_module_cb(lv_event_t *event)
{
    size_t index = (size_t)(uintptr_t)lv_event_get_user_data(event);
    bool enabled = lv_obj_has_state(lv_event_get_target(event), LV_STATE_CHECKED);
    esp_err_t err = bt_debug_set(index, enabled);
    if (err != ESP_OK) ESP_LOGW(TAG, "Cannot change debug module: %s", esp_err_to_name(err));
}

static void debug_sd_cb(lv_event_t *event)
{
    bt_debug_sd_set(lv_obj_has_state(lv_event_get_target(event), LV_STATE_CHECKED));
}

static void debug_all_off_cb(lv_event_t *event)
{
    (void)event;
    bt_debug_all_off();
}

void bt_ui_debug_refresh(bt_ui_t *ui)
{
    ui_debug_snapshot_t snapshot;
    portENTER_CRITICAL(&ui->debug_mux);
    snapshot = ui->debug_snapshot;
    portEXIT_CRITICAL(&ui->debug_mux);
    if (ui->connected && !snapshot.connected) {
        bt_ui_media_set_playing(ui->media, false);
        bt_ui_media_set_track(ui->media, "", "");
    }
    ui->connected = snapshot.connected;
    for (size_t i = 0; i < bt_debug_module_count(); ++i) {
        bool requested = bt_debug_enabled(i);
        bool effective = bt_debug_effective(i);
        bt_ui_switch_state(ui->debug_switches[i], requested);
        lv_label_set_text_fmt(ui->debug_levels[i], "%s: %s", bt_debug_module_at(i)->key,
                             !bt_debug_supported(i) ? "NOT COMPILED" :
                             bt_debug_overridden(i) ? "RAW TAG OVERRIDE" :
                             requested && effective ? "DEBUG (4)" :
                             requested || effective ? "RAW TAG OVERRIDE" : "INFO / WARN");
    }
    bt_debug_status_t status;
    bt_debug_get_status(&status);
    bt_ui_switch_state(ui->debug_sd_switch, status.sd_requested);
    lv_label_set_text_fmt(ui->debug_status,
                         "SD: %s | bytes=%" PRIu64 " dropped=%" PRIu32 " truncated=%" PRIu32 " errno=%d\n"
                         "Settings: %s | Link: %s | Stream: %s | Playback: %s",
                         bt_debug_sd_state_name(status.sd_state), status.bytes_written,
                         status.dropped, status.truncated, status.sd_errno,
                         status.settings_error != ESP_OK ? "SAVE ERROR" : status.settings_pending ? "SAVING" : "SAVED",
                         snapshot.connected ? "CONNECTED" : "DISCONNECTED",
                         debug_stream_state(snapshot.stream_state), debug_playback_state(snapshot.playback_state));
}

void bt_ui_debug_create(bt_ui_t *ui, lv_obj_t *parent, const char *device_name)
{
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x102840), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(parent, lv_color_hex(0xe8eef4), 0);
    lv_obj_set_style_text_font(parent, BT_UI_FONT_SMALL, 0);
    lv_obj_t *name = lv_label_create(parent);
    lv_label_set_text_fmt(name, "Debug controls: %s", device_name);
    lv_obj_set_pos(name, 16, 8);
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_size(button, 150, 36);
    lv_obj_align(button, LV_ALIGN_TOP_RIGHT, -16, 0);
    lv_obj_add_event_cb(button, debug_all_off_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *button_label = lv_label_create(button);
    lv_label_set_text(button_label, "All DEBUG off");
    lv_obj_center(button_label);
    lv_obj_t *sd_label = lv_label_create(parent);
    lv_label_set_text(sd_label, "Save ESP text logs to SD (serial stays on)");
    lv_obj_set_pos(sd_label, 16, 50);
    ui->debug_sd_switch = lv_switch_create(parent);
    lv_obj_set_size(ui->debug_sd_switch, 64, 30);
    lv_obj_set_pos(ui->debug_sd_switch, BT_UI_WIDTH - 94, 44);
    lv_obj_add_event_cb(ui->debug_sd_switch, debug_sd_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_t *list = lv_obj_create(parent);
    lv_obj_set_pos(list, 16, 88);
    lv_obj_set_size(list, BT_UI_WIDTH - 32, CONTENT_H - 170);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x102840), 0);
    lv_obj_set_style_text_color(list, lv_color_hex(0xf4f7fb), 0);
    lv_obj_set_style_pad_all(list, 8, 0);
    lv_obj_set_style_pad_column(list, 8, 0);
    lv_obj_set_style_pad_row(list, 8, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    for (size_t i = 0; i < bt_debug_module_count(); ++i) {
        const bt_debug_module_t *module = bt_debug_module_at(i);
        lv_obj_t *row = lv_obj_create(list);
        lv_obj_set_size(row, 364, 68);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x162f47), 0);
        /* Cards own their theme text style; the page foreground is not enough. */
        lv_obj_set_style_text_color(row, lv_color_hex(0xf4f7fb), 0);
        lv_obj_set_style_pad_all(row, 8, 0);
        lv_obj_t *title = lv_label_create(row);
        lv_label_set_text(title, module->title);
        lv_obj_set_style_text_color(title, lv_color_hex(0xf4f7fb), 0);
        lv_obj_set_width(title, 270);
        lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);
        ui->debug_levels[i] = lv_label_create(row);
        lv_obj_set_style_text_font(ui->debug_levels[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(ui->debug_levels[i], lv_color_hex(0xc9daea), 0);
        lv_obj_align(ui->debug_levels[i], LV_ALIGN_BOTTOM_LEFT, 0, 0);
        ui->debug_switches[i] = lv_switch_create(row);
        lv_obj_set_size(ui->debug_switches[i], 56, 28);
        lv_obj_align(ui->debug_switches[i], LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_add_event_cb(ui->debug_switches[i], debug_module_cb, LV_EVENT_VALUE_CHANGED, (void *)(uintptr_t)i);
        if (!bt_debug_supported(i)) lv_obj_add_state(ui->debug_switches[i], LV_STATE_DISABLED);
    }
    ui->debug_status = lv_label_create(parent);
    lv_obj_set_pos(ui->debug_status, 16, CONTENT_H - 72);
    lv_obj_set_width(ui->debug_status, BT_UI_WIDTH - 32);
    lv_obj_set_style_text_font(ui->debug_status, &lv_font_montserrat_14, 0);
}

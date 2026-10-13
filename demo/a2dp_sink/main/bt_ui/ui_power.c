/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bt_ui_internal.h"

void bt_ui_power_refresh(bt_ui_t *ui)
{
    bt_power_status_t status = {.state = BT_POWER_UNAVAILABLE};
    if (ui->power_config.power_status_cb)
        ui->power_config.power_status_cb(&status, ui->power_config.power_ctx);
    bt_ui_switch_state(ui->power_switch, status.controller_enabled);
    bool busy = status.state == BT_POWER_STARTING || status.state == BT_POWER_STOPPING;
    if (busy || status.state == BT_POWER_UNAVAILABLE || !ui->power_config.power_request_cb)
        lv_obj_add_state(ui->power_switch, LV_STATE_DISABLED);
    else lv_obj_remove_state(ui->power_switch, LV_STATE_DISABLED);
    if (status.state == BT_POWER_ERROR) lv_obj_remove_flag(ui->power_retry, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(ui->power_retry, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text_fmt(ui->power_status,
        "Bluetooth: %s\nController: %s | Step: %s\nOperation: %s | Request: %s",
        bt_power_state_name(status.state), status.controller_enabled ? "ENABLED" : "DISABLED",
        bt_power_step_name(status.step), esp_err_to_name(status.error), esp_err_to_name(ui->power_request_error));
    if (ui->last_power_state != status.state && (status.state == BT_POWER_STOPPING || status.state == BT_POWER_OFF)) {
        bt_ui_media_set_playing(ui->media, false);
        bt_ui_media_set_track(ui->media, "", "");
        bt_ui_post_cover(ui, NULL, 0);
    }
    ui->last_power_state = status.state;
}
static void power_switch_cb(lv_event_t *event)
{
    bt_ui_t *ui = lv_event_get_user_data(event);
    if (ui->power_config.power_request_cb) {
        bool enabled = lv_obj_has_state(ui->power_switch, LV_STATE_CHECKED);
        ui->power_request_error = ui->power_config.power_request_cb(enabled, ui->power_config.power_ctx);
    }
    /* A checked switch is a controller snapshot, never an optimistic promise. */
    bt_ui_power_refresh(ui);
}
static void power_retry_cb(lv_event_t *event)
{
    bt_ui_t *ui = lv_event_get_user_data(event);
    if (ui->power_config.power_request_cb)
        ui->power_request_error = ui->power_config.power_request_cb(false, ui->power_config.power_ctx);
    bt_ui_power_refresh(ui);
}
void bt_ui_power_create(bt_ui_t *ui, lv_obj_t *parent, const bt_ui_config_t *config)
{
    if (config) ui->power_config = *config;
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x102840), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(parent, lv_color_hex(0xf4f7fb), 0);
    lv_obj_set_style_text_font(parent, BT_UI_FONT_SMALL, 0);
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, "Bluetooth power");
    lv_obj_set_pos(label, 24, 24);
    ui->power_switch = lv_switch_create(parent);
    lv_obj_set_size(ui->power_switch, 80, 40);
    lv_obj_set_pos(ui->power_switch, BT_UI_WIDTH - 120, 24);
    lv_obj_add_event_cb(ui->power_switch, power_switch_cb, LV_EVENT_VALUE_CHANGED, ui);
    ui->power_status = lv_label_create(parent);
    lv_obj_set_pos(ui->power_status, 24, 100);
    lv_obj_set_width(ui->power_status, BT_UI_WIDTH - 48);
    lv_obj_set_style_text_font(ui->power_status, &lv_font_montserrat_20, 0);
    label = lv_label_create(parent);
    lv_label_set_text(label, "OFF disconnects the phone. Wait for OFF, then turn ON.\n"
                           "Display, console and log settings stay active.\n"
                           "ERROR: retry OFF before ON. A blocked stack may still need reset.");
    lv_obj_set_pos(label, 24, 240);
    lv_obj_set_width(label, BT_UI_WIDTH - 48);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    ui->power_retry = lv_button_create(parent);
    lv_obj_set_pos(ui->power_retry, 24, 335);
    lv_obj_set_size(ui->power_retry, 240, 48);
    label = lv_label_create(ui->power_retry);
    lv_label_set_text(label, "Retry safe OFF");
    lv_obj_center(label);
    lv_obj_add_event_cb(ui->power_retry, power_retry_cb, LV_EVENT_CLICKED, ui);
    bt_ui_power_refresh(ui);
}

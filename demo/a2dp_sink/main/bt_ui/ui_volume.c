/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bt_ui_internal.h"

#define VOL_BAR_W 6
#define VOL_BAR_H (BT_UI_HEIGHT / 5)
#define VOL_BAR_RIGHT_GAP 12
static lv_timer_t *s_vol_bar_hide_timer = NULL;

static void vol_bar_hide_timer_cb(lv_timer_t *t)
{
    lv_obj_t *bar = (lv_obj_t *)lv_timer_get_user_data(t);
    if (bar != NULL) {
        lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
    }
    lv_timer_del(t);
    s_vol_bar_hide_timer = NULL;
}

lv_obj_t *bt_ui_volume_bar_create(lv_obj_t *parent)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, VOL_BAR_W, VOL_BAR_H);
    lv_obj_align(cont, LV_ALIGN_RIGHT_MID, -VOL_BAR_RIGHT_GAP, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_radius(cont, 2, 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x505060), 0);

    lv_obj_t *fill = lv_obj_create(cont);
    lv_obj_remove_style_all(fill);
    lv_obj_set_width(fill, VOL_BAR_W);
    lv_obj_set_height(fill, 0);
    lv_obj_align(fill, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_clear_flag(fill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(fill, 2, 0);
    lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(fill, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_border_width(fill, 0, 0);
    lv_obj_set_style_pad_all(fill, 0, 0);

    lv_obj_set_user_data(cont, fill);
    lv_obj_add_flag(cont, LV_OBJ_FLAG_HIDDEN);
    return cont;
}

void bt_ui_volume_bar_set_level(lv_obj_t *vol_bar, int level_percent)
{
    if (vol_bar == NULL) {
        return;
    }
    if (level_percent < 0) {
        level_percent = 0;
    }
    if (level_percent > 100) {
        level_percent = 100;
    }
    lv_obj_t *fill = (lv_obj_t *)lv_obj_get_user_data(vol_bar);
    if (fill == NULL) {
        return;
    }
    int32_t h = lv_obj_get_height(vol_bar);
    int32_t fill_h = (int32_t)((int64_t)h * level_percent / 100);
    if (fill_h < 0) {
        fill_h = 0;
    }
    lv_obj_set_height(fill, (lv_coord_t)fill_h);
    lv_obj_align(fill, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_clear_flag(vol_bar, LV_OBJ_FLAG_HIDDEN);
    if (s_vol_bar_hide_timer != NULL) {
        lv_timer_reset(s_vol_bar_hide_timer);
    } else {
        s_vol_bar_hide_timer = lv_timer_create(vol_bar_hide_timer_cb, 1000, vol_bar);
        lv_timer_set_repeat_count(s_vol_bar_hide_timer, 1);
    }
}

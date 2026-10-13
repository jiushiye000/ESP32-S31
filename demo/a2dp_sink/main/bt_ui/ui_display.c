/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_board_manager.h"
#include "esp_board_manager_defs.h"
#include "dev_display_lcd.h"
#include "dev_lcd_touch.h"
#include "bt_ui.h"

#define LVGL_DRAW_BUF_LINES (BT_UI_HEIGHT / 2)
static const char *TAG = "BT_UI";

esp_err_t bt_ui_init(void)
{
    dev_display_lcd_handles_t *lcd_handles = NULL;
    dev_display_lcd_config_t *lcd_cfg = NULL;
    esp_err_t ret = esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD, (void **)&lcd_handles);
    if (ret != ESP_OK || lcd_handles == NULL) {
        ESP_LOGE(TAG, "Failed to get LCD handle: %s", esp_err_to_name(ret));
        return ret == ESP_OK ? ESP_ERR_INVALID_STATE : ret;
    }
    ret = esp_board_manager_get_device_config(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD, (void **)&lcd_cfg);
    if (ret != ESP_OK || lcd_cfg == NULL) {
        ESP_LOGE(TAG, "Failed to get LCD config: %s", esp_err_to_name(ret));
        return ret == ESP_OK ? ESP_ERR_INVALID_STATE : ret;
    }

    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "LVGL port init failed");

    lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = lcd_handles->io_handle,
        .panel_handle = lcd_handles->panel_handle,
        .buffer_size = lcd_cfg->lcd_width * LVGL_DRAW_BUF_LINES,
        .double_buffer = true,
        .hres = lcd_cfg->lcd_width,
        .vres = lcd_cfg->lcd_height,
        .monochrome = false,
        .rotation = {
            .swap_xy = lcd_cfg->swap_xy,
            .mirror_x = lcd_cfg->mirror_x,
            .mirror_y = lcd_cfg->mirror_y,
        },
#if LVGL_VERSION_MAJOR >= 9
        .color_format = LV_COLOR_FORMAT_RGB565,
#endif  /* LVGL_VERSION_MAJOR >= 9 */
        .flags = {
            .buff_dma = false,
            .buff_spiram = true,
        },
    };

    lv_disp_t *disp = NULL;
    if (strcmp(lcd_cfg->sub_type, "rgb") == 0) {
#ifdef CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_RGB_SUPPORT
        const lvgl_port_display_rgb_cfg_t rgb_cfg = {
            .flags = {
                .bb_mode = lcd_cfg->sub_cfg.rgb.panel_config.bounce_buffer_size_px > 0,
                .avoid_tearing = lcd_cfg->sub_cfg.rgb.panel_config.num_fbs > 1,
            },
        };
        disp_cfg.flags.direct_mode = rgb_cfg.flags.avoid_tearing;
        if (rgb_cfg.flags.avoid_tearing) {
            /* esp_lvgl_port borrows the two LCD framebuffers and waits for VSYNC. */
            disp_cfg.buffer_size = lcd_cfg->lcd_width * lcd_cfg->lcd_height;
            disp_cfg.double_buffer = false;
            disp_cfg.flags.full_refresh = false;
            disp_cfg.flags.sw_rotate = false;
        } else {
            ESP_LOGW(TAG, "RGB anti-tearing OFF: regenerate the rgb_antitear board amend");
        }
        ESP_LOGI(TAG, "RGB LCD: framebuffers=%d, avoid_tearing=%d, direct_mode=%d",
                 lcd_cfg->sub_cfg.rgb.panel_config.num_fbs, rgb_cfg.flags.avoid_tearing,
                 disp_cfg.flags.direct_mode);
        disp = lvgl_port_add_disp_rgb(&disp_cfg, &rgb_cfg);
#endif  /* CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_RGB_SUPPORT */
    } else {
        disp = lvgl_port_add_disp(&disp_cfg);
    }
    if (disp == NULL) {
        ESP_LOGE(TAG, "Failed to add LVGL display");
        return ESP_FAIL;
    }

#ifdef CONFIG_ESP_BOARD_DEV_LCD_TOUCH_SUPPORT
    dev_lcd_touch_handles_t *touch_handles = NULL;
    ret = esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_LCD_TOUCH, (void **)&touch_handles);
    if (ret == ESP_OK && touch_handles != NULL && touch_handles->touch_handle != NULL) {
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = disp,
            .handle = touch_handles->touch_handle,
        };
        if (lvgl_port_add_touch(&touch_cfg) == NULL) {
            ESP_LOGW(TAG, "Failed to add LVGL touch input");
        }
    } else {
        ESP_LOGW(TAG, "LCD touch unavailable: %s", esp_err_to_name(ret));
    }
#endif  /* CONFIG_ESP_BOARD_DEV_LCD_TOUCH_SUPPORT */
    return ESP_OK;
}

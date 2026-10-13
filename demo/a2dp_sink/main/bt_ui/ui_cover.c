/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "jpeg_decoder.h"
#include "bt_ui_internal.h"
#if LVGL_VERSION_MAJOR >= 9
#include "src/misc/cache/instance/lv_image_cache.h"
#endif

#define UI_COVER_MAX_BYTES (512 * 1024)
static const char *TAG = "BT_UI";

typedef struct {
    uint8_t *data;
    esp_jpeg_image_output_t image;
} ui_decoded_cover_t;

static ui_decoded_cover_t ui_cover_decode(const uint8_t *data, size_t size);
static void ui_cover_install(lv_obj_t *media, ui_decoded_cover_t *cover);

void bt_ui_cover_task(void *arg)
{
    bt_ui_t *ui = (bt_ui_t *)arg;
    ui_cover_msg_t msg = {0};
    while (true) {
        if (xQueueReceive(ui->cover_queue, &msg, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        portENTER_CRITICAL(&ui->cover_mux);
        bool current = msg.generation == ui->cover_generation;
        portEXIT_CRITICAL(&ui->cover_mux);
        ui_decoded_cover_t decoded = {0};
        if (current && msg.data != NULL) {
            decoded = ui_cover_decode(msg.data, msg.size); /* Blocking decode outside LVGL. */
        }
        free(msg.data);
        lvgl_port_lock(0);
        portENTER_CRITICAL(&ui->cover_mux);
        current = msg.generation == ui->cover_generation;
        portEXIT_CRITICAL(&ui->cover_mux);
        if (current && ui->media != NULL) ui_cover_install(ui->media, &decoded);
        lvgl_port_unlock();
        free(decoded.data); /* Install takes ownership on success. */
    }
}

void bt_ui_post_cover(bt_ui_t *ui, const uint8_t *data, size_t size)
{
    if (ui == NULL || ui->cover_queue == NULL) {
        return;
    }
    bool clear = data == NULL || size == 0;
    portENTER_CRITICAL(&ui->cover_mux);
    if (clear) ++ui->cover_generation;
    uint32_t generation = ui->cover_generation;
    portEXIT_CRITICAL(&ui->cover_mux);
    uint8_t *copy = NULL;
    if (!clear) {
        if (size > UI_COVER_MAX_BYTES) {
            ESP_LOGW(TAG, "Cover art exceeds UI input limit: %u", (unsigned)size);
            return;
        }
        copy = heap_caps_malloc_prefer(size, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
        if (copy == NULL) {
            ESP_LOGW(TAG, "No memory for cover art copy, size %u", (unsigned)size);
            return;
        }
        memcpy(copy, data, size);
    }
    ui_cover_msg_t msg = {
        .data = copy,
        .size = clear ? 0 : size,
        .generation = generation,
    };
    if (xQueueSend(ui->cover_queue, &msg, 0) != pdTRUE) {
        /* Bounded, non-blocking latest-update policy; never leak evicted payloads. */
        ui_cover_msg_t old;
        if (xQueueReceive(ui->cover_queue, &old, 0) == pdTRUE) free(old.data);
        if (xQueueSend(ui->cover_queue, &msg, 0) != pdTRUE) free(copy);
    }
}

/* Guard the official JPEG info reader's marker accesses before calling it.
 * This is bounds validation, not a replacement JPEG decoder. Only baseline SOF0
 * is supported by esp_jpeg; reject dimensions that cannot fit even at 1/8 scale. */
static bool ui_cover_header_valid(const uint8_t *data, size_t size)
{
    if (data == NULL || size < 4 || size > UI_COVER_MAX_BYTES || data[0] != 0xff || data[1] != 0xd8) return false;
    unsigned components = 0;
    for (size_t pos = 2; pos <= size - 4;) {
        uint16_t len = ((uint16_t)data[pos + 2] << 8) | data[pos + 3];
        if (data[pos] != 0xff || len <= 2 || (size_t)len > size - pos - 2) return false;
        if (data[pos + 1] == 0xc0) {
            if (components != 0 || len < 8 || data[pos + 4] != 8) return false;
            unsigned h = ((unsigned)data[pos + 5] << 8) | data[pos + 6];
            unsigned w = ((unsigned)data[pos + 7] << 8) | data[pos + 8];
            components = data[pos + 9];
            if ((components != 1 && components != 3) || len != 8 + 3 * components ||
                h == 0 || w == 0 || h > COVER_IMG_SIZE * 8 || w > COVER_IMG_SIZE * 8) return false;
        } else if (data[pos + 1] == 0xda) {
            /* Validate every pre-scan segment, not just those before SOF0.
             * Stop at SOS: entropy bytes are not length-prefixed segments. */
            if (components == 0 || len != 6 + 2 * components || data[pos + 4] != components) return false;
            return pos + 2 + len < size;
        } else if (data[pos + 1] == 0xdd) {
            if (len != 4) return false;
        } else if (data[pos + 1] != 0xc4 && data[pos + 1] != 0xdb && data[pos + 1] != 0xfe &&
                   (data[pos + 1] < 0xe0 || data[pos + 1] > 0xef)) {
            return false; /* No progressive/duplicate frame or standalone marker. */
        }
        pos += 2 + len;
    }
    return false;
}

static ui_decoded_cover_t ui_cover_decode(const uint8_t *data, size_t size)
{
    ui_decoded_cover_t result = {0};
    if (!ui_cover_header_valid(data, size)) {
        ESP_LOGW(TAG, "Cover art rejected: invalid/oversized baseline JPEG header");
        return result;
    }
    esp_jpeg_image_cfg_t cfg = {
        .indata = (uint8_t *)data,
        .indata_size = (uint32_t)size,
        .outbuf = NULL,
        .outbuf_size = 0,
        .out_format = JPEG_IMAGE_FORMAT_RGB565,
        .out_scale = JPEG_IMAGE_SCALE_0,
        .flags = {.swap_color_bytes = 1},
        .advanced = {.working_buffer = NULL, .working_buffer_size = 0},
        .priv = {0},
    };
    esp_jpeg_image_output_t info;
    esp_err_t err = esp_jpeg_get_image_info(&cfg, &info);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Cover art info failed: %s", esp_err_to_name(err));
        return result;
    }
    uint32_t src_w = (uint32_t)info.width;
    uint32_t src_h = (uint32_t)info.height;
    ESP_LOGD(TAG, "Set cover image: jpeg info %" PRIu32 "x%" PRIu32 " output_len=%u", src_w, src_h,
             (unsigned)info.output_len);
    esp_jpeg_image_scale_t scale = JPEG_IMAGE_SCALE_0;
    if (src_w > COVER_IMG_SIZE || src_h > COVER_IMG_SIZE) {
        if (src_w / 2 <= COVER_IMG_SIZE && src_h / 2 <= COVER_IMG_SIZE) {
            scale = JPEG_IMAGE_SCALE_1_2;
        } else if (src_w / 4 <= COVER_IMG_SIZE && src_h / 4 <= COVER_IMG_SIZE) {
            scale = JPEG_IMAGE_SCALE_1_4;
        } else {
            scale = JPEG_IMAGE_SCALE_1_8;
        }
    }
    cfg.out_scale = scale;
    if (esp_jpeg_get_image_info(&cfg, &info) != ESP_OK) {
        return result;
    }
    /* get_image_info returns unscaled dimensions, but scaled output_len. */
    unsigned divisor = 1U << (unsigned)scale;
    if (info.width / divisor == 0 || info.height / divisor == 0 ||
        info.width / divisor > COVER_IMG_SIZE || info.height / divisor > COVER_IMG_SIZE ||
        info.output_len == 0 || info.output_len > COVER_IMG_SIZE * COVER_IMG_SIZE * 2) return result;
    uint8_t *outbuf = heap_caps_malloc_prefer(info.output_len, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
    if (outbuf == NULL) {
        ESP_LOGW(TAG, "No memory for decoded cover: %u", (unsigned)info.output_len);
        return result;
    }
    cfg.outbuf = outbuf;
    cfg.outbuf_size = (uint32_t)info.output_len;
    esp_jpeg_image_output_t outimg;
    err = esp_jpeg_decode(&cfg, &outimg);
    if (err != ESP_OK || outimg.width == 0 || outimg.height == 0 ||
        outimg.width > COVER_IMG_SIZE || outimg.height > COVER_IMG_SIZE ||
        outimg.output_len > info.output_len || outimg.output_len != (size_t)outimg.width * outimg.height * 2) {
        ESP_LOGW(TAG, "Cover art decode failed or invalid output: %s", esp_err_to_name(err));
        free(outbuf);
        return result;
    }
    ESP_LOGD(TAG, "Set cover image: decoded %ux%u output_len=%u", (unsigned)outimg.width,
             (unsigned)outimg.height, (unsigned)outimg.output_len);
    result.data = outbuf;
    result.image = outimg;
    return result;
}

/* Requires LVGL lock; consumes decoded pixels, never encoded caller data. */
static void ui_cover_install(lv_obj_t *media_root, ui_decoded_cover_t *cover)
{
    bt_ui_media_refs_t *refs = media_root ? lv_obj_get_user_data(media_root) : NULL;
    if (refs == NULL || refs->cover_img == NULL) return;
    lv_obj_add_flag(refs->cover_img, LV_OBJ_FLAG_HIDDEN);
#if LVGL_VERSION_MAJOR >= 9
    lv_image_cache_drop(&refs->cover_dsc);
#else
    lv_img_cache_invalidate_src(&refs->cover_img_dsc);
#endif
    if (refs->cover_data != NULL) {
        free(refs->cover_data);
    }
    refs->cover_data = cover->data;
    refs->cover_size = cover->image.output_len;
    cover->data = NULL;
    if (refs->cover_data == NULL) {
        lv_obj_clear_flag(refs->cover_placeholder, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    esp_jpeg_image_output_t outimg = cover->image;

    if (refs->cover_img != NULL) {
        uint32_t stride = (uint32_t)outimg.width * 2;  /* RGB565 = 2 bytes per pixel */
#if LVGL_VERSION_MAJOR >= 9
        memset(&refs->cover_dsc, 0, sizeof(refs->cover_dsc));
        refs->cover_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
        refs->cover_dsc.header.cf = LV_COLOR_FORMAT_RGB565_SWAPPED;
        refs->cover_dsc.header.flags = 0;
        refs->cover_dsc.header.w = (int32_t)outimg.width;
        refs->cover_dsc.header.h = (int32_t)outimg.height;
        refs->cover_dsc.header.stride = (uint32_t)stride;
        refs->cover_dsc.data_size = (uint32_t)outimg.output_len;
        refs->cover_dsc.data = refs->cover_data;
        lv_image_set_src(refs->cover_img, &refs->cover_dsc);
#else
        memset(&refs->cover_img_dsc, 0, sizeof(refs->cover_img_dsc));
        refs->cover_img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
        refs->cover_img_dsc.header.w = (int32_t)outimg.width;
        refs->cover_img_dsc.header.h = (int32_t)outimg.height;
        refs->cover_img_dsc.header.stride = (uint32_t)stride;
        refs->cover_img_dsc.data_size = (uint32_t)outimg.output_len;
        refs->cover_img_dsc.data = refs->cover_data;
        lv_img_set_src(refs->cover_img, &refs->cover_img_dsc);
#endif  /* LVGL_VERSION_MAJOR >= 9 */
        lv_obj_clear_flag(refs->cover_img, LV_OBJ_FLAG_HIDDEN);
    }
    if (refs->cover_placeholder != NULL) {
        lv_obj_add_flag(refs->cover_placeholder, LV_OBJ_FLAG_HIDDEN);
    }
    ESP_LOGD(TAG, "Set cover image: done, cover visible %ux%u", (unsigned)outimg.width,
             (unsigned)outimg.height);
}

void bt_ui_media_set_cover_image(lv_obj_t *media_root, const uint8_t *data, size_t size)
{
    ui_decoded_cover_t decoded = {0};
    if (data != NULL && size != 0) decoded = ui_cover_decode(data, size);
    ui_cover_install(media_root, &decoded);
    free(decoded.data);
}

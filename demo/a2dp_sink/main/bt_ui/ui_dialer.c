/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bt_ui_internal.h"

#define DIALER_BUF_SIZE 32
#define DIALER_KEYPAD_SZ 92
#define DIALER_KEY_GAP 16

/**
 * @brief  Cached LVGL widgets used by the dialer page.
 */
typedef struct {
    lv_obj_t *number_label;    /*!< Dialed number and call-state label */
    lv_obj_t *call_btn;        /*!< Dial/end-call button */
    lv_obj_t *call_btn_label;  /*!< Icon label inside call_btn */
    lv_obj_t *answer_btn;      /*!< Answer button shown for incoming calls */
    lv_obj_t *back_btn;        /*!< Backspace button */
    void     *call_ctx;        /*!< Pointer to dialer_call_ctx_t */
} bt_ui_dialer_refs_t;

/**
 * @brief  Dialer callback context and current call state.
 */
typedef struct {
    void (*call_cb)(const char *number, void *ctx);  /*!< Dial callback */
    void *call_cb_ctx;                               /*!< Context passed to call_cb */
    void (*end_call_cb)(void *ctx);                  /*!< End-call callback */
    void *end_call_cb_ctx;                           /*!< Context passed to end_call_cb */
    void (*answer_call_cb)(void *ctx);               /*!< Answer-call callback */
    void *answer_call_cb_ctx;                        /*!< Context passed to answer_call_cb */
    bool  in_call;                                   /*!< True while the dialer is in a call state */
} dialer_call_ctx_t;

static char dialer_number_buf[DIALER_BUF_SIZE];
static lv_obj_t *dialer_number_label;

static void dialer_digit_click_cb(lv_event_t *e)
{
    lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
    lv_obj_t *lbl = (lv_obj_t *)lv_obj_get_child(btn, 0);
    const char *txt = lbl ? lv_label_get_text(lbl) : NULL;
    if (txt == NULL || dialer_number_label == NULL) {
        return;
    }
    size_t len = lv_strlen(dialer_number_buf);
    if (len + 2 >= DIALER_BUF_SIZE) {
        return;
    }
    if (txt[0] != '\0') {
        dialer_number_buf[len] = txt[0];
        dialer_number_buf[len + 1] = '\0';
    }
    lv_label_set_text(dialer_number_label, dialer_number_buf[0] ? dialer_number_buf : "Enter Number");
}

static void dialer_back_click_cb(lv_event_t *e)
{
    (void)e;
    if (dialer_number_label == NULL) {
        return;
    }
    size_t len = lv_strlen(dialer_number_buf);
    if (len > 0) {
        dialer_number_buf[len - 1] = '\0';
    }
    lv_label_set_text(dialer_number_label, dialer_number_buf[0] ? dialer_number_buf : "Enter Number");
}

static void dialer_call_click_cb(lv_event_t *e)
{
    lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
    dialer_call_ctx_t *ctx = (dialer_call_ctx_t *)lv_obj_get_user_data(btn);
    if (ctx == NULL) {
        return;
    }
    if (ctx->in_call) {
        if (ctx->end_call_cb) {
            ctx->end_call_cb(ctx->end_call_cb_ctx);
        }
        return;
    }
    if (ctx->call_cb) {
        ctx->call_cb(dialer_number_buf, ctx->call_cb_ctx);
    }
}

static void dialer_answer_click_cb(lv_event_t *e)
{
    lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
    dialer_call_ctx_t *ctx = (dialer_call_ctx_t *)lv_obj_get_user_data(btn);
    if (ctx && ctx->answer_call_cb) {
        ctx->answer_call_cb(ctx->answer_call_cb_ctx);
    }
}

lv_obj_t *bt_ui_dialer_create(lv_obj_t *parent,
                              void (*call_cb)(const char *number, void *ctx), void *call_cb_ctx,
                              void (*end_call_cb)(void *ctx), void *end_call_cb_ctx,
                              void (*answer_call_cb)(void *ctx), void *answer_call_cb_ctx)
{
    dialer_number_buf[0] = '\0';
    dialer_number_label = NULL;

    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, BT_UI_WIDTH, CONTENT_H);
    lv_obj_center(root);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x102840), 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    dialer_number_label = lv_label_create(root);
    lv_label_set_text(dialer_number_label, "Enter Number");
    lv_obj_set_style_text_color(dialer_number_label, lv_color_hex(0xe8eef4), 0);
    lv_obj_set_width(dialer_number_label, BT_UI_WIDTH / 2 - 64);
    lv_label_set_long_mode(dialer_number_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(dialer_number_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(dialer_number_label, LV_ALIGN_TOP_RIGHT, -44, 88);
    lv_obj_set_style_text_font(dialer_number_label, BT_UI_FONT_LARGE, 0);

    static const char *keys[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "0", "#"};
    const int cols = 3, rows = 4;
    const int key_sz = DIALER_KEYPAD_SZ;
    int start_x = 46;
    int start_y = (CONTENT_H - (rows * key_sz + (rows - 1) * DIALER_KEY_GAP)) / 2;
    const lv_color_t key_gray = lv_color_hex(0x6a6a6a);

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            int i = r * cols + c;
#if LVGL_VERSION_MAJOR >= 9
            lv_obj_t *btn = lv_button_create(root);
#else
            lv_obj_t *btn = lv_btn_create(root);
#endif  /* LVGL_VERSION_MAJOR >= 9 */
            lv_obj_set_size(btn, key_sz, key_sz);
            lv_obj_set_style_shadow_width(btn, 0, 0);
            lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(btn, key_gray, 0);
            lv_obj_set_pos(btn, start_x + c * (key_sz + DIALER_KEY_GAP), start_y + r * (key_sz + DIALER_KEY_GAP));
            lv_obj_t *lbl = lv_label_create(btn);
            lv_label_set_text(lbl, keys[i]);
            lv_obj_center(lbl);
            lv_obj_set_style_text_color(lbl, lv_color_hex(0xe8eef4), 0);
            lv_obj_set_style_text_font(lbl, BT_UI_FONT_LARGE, 0);
            lv_obj_add_event_cb(btn, dialer_digit_click_cb, LV_EVENT_CLICKED, NULL);
        }
    }

#if LVGL_VERSION_MAJOR >= 9
    lv_obj_t *call_btn = lv_button_create(root);
#else
    lv_obj_t *call_btn = lv_btn_create(root);
#endif  /* LVGL_VERSION_MAJOR >= 9 */
    lv_obj_set_size(call_btn, key_sz, key_sz);
    lv_obj_set_style_shadow_width(call_btn, 0, 0);
    lv_obj_set_style_radius(call_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(call_btn, lv_color_hex(0x2E7D32), 0);
    lv_obj_set_pos(call_btn, BT_UI_WIDTH - 284, CONTENT_H - key_sz - 54);
    static dialer_call_ctx_t s_dialer_call_ctx;
    s_dialer_call_ctx.call_cb = call_cb;
    s_dialer_call_ctx.call_cb_ctx = call_cb_ctx;
    s_dialer_call_ctx.end_call_cb = end_call_cb;
    s_dialer_call_ctx.end_call_cb_ctx = end_call_cb_ctx;
    s_dialer_call_ctx.answer_call_cb = answer_call_cb;
    s_dialer_call_ctx.answer_call_cb_ctx = answer_call_cb_ctx;
    s_dialer_call_ctx.in_call = false;
    lv_obj_set_user_data(call_btn, &s_dialer_call_ctx);
    lv_obj_add_event_cb(call_btn, dialer_call_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *call_lbl = lv_label_create(call_btn);
    lv_label_set_text(call_lbl, LV_SYMBOL_CALL);
    lv_obj_center(call_lbl);
    lv_obj_set_style_text_color(call_lbl, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(call_lbl, BT_UI_FONT_ICON, 0);

#if LVGL_VERSION_MAJOR >= 9
    lv_obj_t *back_btn = lv_button_create(root);
#else
    lv_obj_t *back_btn = lv_btn_create(root);
#endif  /* LVGL_VERSION_MAJOR >= 9 */
    lv_obj_set_size(back_btn, key_sz, key_sz);
    lv_obj_set_style_shadow_width(back_btn, 0, 0);
    lv_obj_set_style_radius(back_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x795548), 0);
    lv_obj_set_pos(back_btn, BT_UI_WIDTH - 158, CONTENT_H - key_sz - 54);
    lv_obj_t *back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, LV_SYMBOL_BACKSPACE);
    lv_obj_center(back_lbl);
    lv_obj_set_style_text_color(back_lbl, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(back_lbl, BT_UI_FONT_ICON, 0);
    lv_obj_add_event_cb(back_btn, dialer_back_click_cb, LV_EVENT_CLICKED, NULL);

#if LVGL_VERSION_MAJOR >= 9
    lv_obj_t *answer_btn = lv_button_create(root);
#else
    lv_obj_t *answer_btn = lv_btn_create(root);
#endif  /* LVGL_VERSION_MAJOR >= 9 */
    lv_obj_set_size(answer_btn, key_sz, key_sz);
    lv_obj_set_style_shadow_width(answer_btn, 0, 0);
    lv_obj_set_style_radius(answer_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(answer_btn, lv_color_hex(0x2E7D32), 0);
    lv_obj_set_pos(answer_btn, BT_UI_WIDTH - 158, CONTENT_H - key_sz - 54);
    lv_obj_set_user_data(answer_btn, &s_dialer_call_ctx);
    lv_obj_add_event_cb(answer_btn, dialer_answer_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *answer_lbl = lv_label_create(answer_btn);
    lv_label_set_text(answer_lbl, LV_SYMBOL_CALL);
    lv_obj_center(answer_lbl);
    lv_obj_set_style_text_color(answer_lbl, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(answer_lbl, BT_UI_FONT_ICON, 0);
    lv_obj_add_flag(answer_btn, LV_OBJ_FLAG_HIDDEN);

    static bt_ui_dialer_refs_t s_dialer_refs;
    s_dialer_refs.number_label = dialer_number_label;
    s_dialer_refs.call_btn = call_btn;
    s_dialer_refs.call_btn_label = call_lbl;
    s_dialer_refs.answer_btn = answer_btn;
    s_dialer_refs.back_btn = back_btn;
    s_dialer_refs.call_ctx = &s_dialer_call_ctx;
    lv_obj_set_user_data(root, &s_dialer_refs);

    return root;
}

void bt_ui_dialer_set_call_state(lv_obj_t *dialer_root, int state, const char *number)
{
    if (dialer_root == NULL) {
        return;
    }
    bt_ui_dialer_refs_t *refs = (bt_ui_dialer_refs_t *)lv_obj_get_user_data(dialer_root);
    if (refs == NULL || refs->number_label == NULL) {
        return;
    }
    const char *state_text = "";
    switch (state) {
        case 1:
            state_text = "Incoming";
            break;
        case 2:
            state_text = "Dialing...";
            break;
        case 3:
            state_text = "Ringing...";
            break;
        case 4:
            state_text = "In call";
            break;
        case 5:
        case 6:
        case 7:
            state_text = "Held";
            break;
        default:
            break;
    }
    if (state != 0 && number != NULL && number[0] != '\0') {
        lv_label_set_text_fmt(refs->number_label, "%s  %s", state_text, number);
    } else if (state != 0) {
        lv_label_set_text(refs->number_label, state_text);
    } else {
        dialer_number_buf[0] = '\0';
        lv_label_set_text(refs->number_label, "Enter Number");
    }
    dialer_call_ctx_t *ctx = (dialer_call_ctx_t *)refs->call_ctx;
    if (ctx != NULL) {
        ctx->in_call = state != 0;
    }

    bool incoming = (state == ESP_BT_AUDIO_CALL_STATE_INCOMING);
    bool active = (state != 0);

    if (refs->call_btn != NULL) {
        lv_obj_set_style_bg_color(refs->call_btn, lv_color_hex(active ? 0xc62828 : 0x2E7D32), 0);
    }
    if (refs->call_btn_label != NULL) {
        lv_label_set_text(refs->call_btn_label, active ? LV_SYMBOL_CLOSE : LV_SYMBOL_CALL);
    }

    if (refs->answer_btn != NULL) {
        if (incoming) {
            lv_obj_clear_flag(refs->answer_btn, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(refs->answer_btn, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (refs->back_btn != NULL) {
        if (active) {
            lv_obj_add_flag(refs->back_btn, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(refs->back_btn, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

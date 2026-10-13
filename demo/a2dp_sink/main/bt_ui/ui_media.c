/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bt_ui_internal.h"

/* Landscape (800x480) layout. */
#define COVER_SIZE 360
#define MEDIA_LEFT_PAD 32
#define MEDIA_GAP 32
#define RIGHT_PANEL_X (MEDIA_LEFT_PAD + COVER_SIZE + MEDIA_GAP)
#define RIGHT_PANEL_W (BT_UI_WIDTH - RIGHT_PANEL_X - MEDIA_LEFT_PAD)
#define BTN_ROW_HEIGHT 88
#define BTN_W 92
#define BTN_H 68
#define NAME_AREA_LINES 5
#define NAME_AREA_H 240

#if CONFIG_GMF_EXAMPLE_AUDIO_TECH_LE
LV_IMAGE_DECLARE(speakers_dark_dark_small);
LV_IMAGE_DECLARE(speakers_dark_light_small);
LV_IMAGE_DECLARE(speakers_light_dark_small);
LV_IMAGE_DECLARE(cis_stream_icon);
LV_IMAGE_DECLARE(bis_stream_icon);

/* Source art is 440x260; keep aspect when fitting the zone below the title. */
#define SPEAKER_SRC_W      440
#define SPEAKER_SRC_H      260
#define LE_TITLE_AREA_H    96
#define LE_SPEAKER_ZONE_W  (RIGHT_PANEL_W - 16)
#define LE_SPEAKER_ZONE_H  (COVER_SIZE - LE_TITLE_AREA_H - BTN_ROW_HEIGHT)
#if (LE_SPEAKER_ZONE_W * SPEAKER_SRC_H / SPEAKER_SRC_W) <= LE_SPEAKER_ZONE_H
#define SPEAKER_IMG_W  LE_SPEAKER_ZONE_W
#define SPEAKER_IMG_H  (LE_SPEAKER_ZONE_W * SPEAKER_SRC_H / SPEAKER_SRC_W)
#else
#define SPEAKER_IMG_H  LE_SPEAKER_ZONE_H
#define SPEAKER_IMG_W  (LE_SPEAKER_ZONE_H * SPEAKER_SRC_W / SPEAKER_SRC_H)
#endif  /* (LE_SPEAKER_ZONE_W * SPEAKER_SRC_H / SPEAKER_SRC_W) <= LE_SPEAKER_ZONE_H */
#endif  /* CONFIG_GMF_EXAMPLE_AUDIO_TECH_LE */

#if CONFIG_GMF_EXAMPLE_AUDIO_TECH_LE
static const lv_image_dsc_t *speaker_img_for_location(uint32_t location)
{
    bool left = (location & ESP_BT_AUDIO_AUDIO_LOC_FRONT_LEFT) != 0;
    bool right = (location & ESP_BT_AUDIO_AUDIO_LOC_FRONT_RIGHT) != 0;
    if (left && right) {
        return &speakers_dark_dark_small;
    }
    if (left) {
        return &speakers_dark_light_small;
    }
    if (right) {
        return &speakers_light_dark_small;
    }
    return &speakers_dark_dark_small;
}
#endif  /* CONFIG_GMF_EXAMPLE_AUDIO_TECH_LE */

static void btn_play_pause_click_cb(lv_event_t *e)
{
    lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
    lv_obj_t *root = lv_obj_get_parent(lv_obj_get_parent(lv_obj_get_parent(btn)));
    bt_ui_media_refs_t *refs = root ? (bt_ui_media_refs_t *)lv_obj_get_user_data(root) : NULL;
    if (refs) {
        /* VALUE_CHANGED runs after toggle: checked => user requested play */
        bool want_play = lv_obj_has_state(btn, LV_STATE_CHECKED);
        esp_err_t ret = refs->play_pause_cb ? refs->play_pause_cb(want_play, refs->play_pause_ctx) : ESP_ERR_INVALID_STATE;
        /* Show the accepted request immediately; phone notifications remain authoritative. */
        bt_ui_media_set_playing(root, ret == ESP_OK ? want_play : !want_play);
    }
}

static void btn_prev_click_cb(lv_event_t *e)
{
    (void)e;
    lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
    lv_obj_t *root = lv_obj_get_parent(lv_obj_get_parent(lv_obj_get_parent(btn)));
    bt_ui_media_refs_t *refs = root ? (bt_ui_media_refs_t *)lv_obj_get_user_data(root) : NULL;
    if (refs && refs->prev_cb) {
        refs->prev_cb(refs->prev_next_ctx);
    }
}

static void btn_next_click_cb(lv_event_t *e)
{
    (void)e;
    lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
    lv_obj_t *root = lv_obj_get_parent(lv_obj_get_parent(lv_obj_get_parent(btn)));
    bt_ui_media_refs_t *refs = root ? (bt_ui_media_refs_t *)lv_obj_get_user_data(root) : NULL;
    if (refs && refs->next_cb) {
        refs->next_cb(refs->prev_next_ctx);
    }
}

void bt_ui_media_set_playing(lv_obj_t *media_root, bool playing)
{
    if (media_root == NULL) {
        return;
    }
    bt_ui_media_refs_t *refs = (bt_ui_media_refs_t *)lv_obj_get_user_data(media_root);
    if (refs == NULL || refs->play_btn == NULL) {
        return;
    }
    lv_obj_t *lbl = lv_obj_get_child(refs->play_btn, 0);
    if (lbl != NULL && lv_obj_check_type(lbl, &lv_label_class)) {
        lv_label_set_text(lbl, playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
        if (playing) {
            lv_obj_add_state(refs->play_btn, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(refs->play_btn, LV_STATE_CHECKED);
        }
    }
}

void bt_ui_media_set_track(lv_obj_t *media_root, const char *title, const char *artist)
{
    if (media_root == NULL) {
        return;
    }
    bt_ui_media_refs_t *refs = (bt_ui_media_refs_t *)lv_obj_get_user_data(media_root);
    if (refs == NULL || refs->title_label == NULL) {
        return;
    }
    /* NULL means leave current; UI labels hold the state across partial updates */
    if (title != NULL) {
        lv_label_set_text(refs->title_label, title[0] != '\0' ? title : "No music playing");
    }
    if (refs->artist_label != NULL && artist != NULL) {
        lv_label_set_text(refs->artist_label, artist[0] != '\0' ? artist : "");
    }
}

void bt_ui_media_set_stream_type(lv_obj_t *media_root, bt_ui_media_stream_type_t type)
{
    if (media_root == NULL) {
        return;
    }
    bt_ui_media_refs_t *refs = (bt_ui_media_refs_t *)lv_obj_get_user_data(media_root);
    if (refs == NULL || refs->cover_placeholder == NULL || refs->cover_type_label == NULL) {
        return;
    }

    const lv_image_dsc_t *icon = NULL;
#if CONFIG_GMF_EXAMPLE_AUDIO_TECH_LE
    if (type == BT_UI_MEDIA_STREAM_TYPE_CIS) {
        icon = &cis_stream_icon;
    } else if (type == BT_UI_MEDIA_STREAM_TYPE_BIS) {
        icon = &bis_stream_icon;
    }
#endif  /* CONFIG_GMF_EXAMPLE_AUDIO_TECH_LE */
    if (icon != NULL && refs->cover_type_img != NULL) {
        lv_image_set_src(refs->cover_type_img, icon);
        lv_obj_clear_flag(refs->cover_type_img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(refs->cover_type_label, LV_OBJ_FLAG_HIDDEN);
        if (refs->cover_type_desc_label != NULL) {
            lv_obj_add_flag(refs->cover_type_desc_label, LV_OBJ_FLAG_HIDDEN);
        }
        if (refs->cover_img != NULL) {
            lv_obj_add_flag(refs->cover_img, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        if (refs->cover_type_img != NULL) {
            lv_obj_add_flag(refs->cover_type_img, LV_OBJ_FLAG_HIDDEN);
        }
        lv_label_set_text(refs->cover_type_label, LV_SYMBOL_AUDIO);
        lv_obj_set_style_text_color(refs->cover_type_label, lv_color_hex(0xa8b8c8), 0);
        lv_obj_clear_flag(refs->cover_type_label, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_clear_flag(refs->cover_placeholder, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t *bt_ui_media_create(lv_obj_t *parent,
                             esp_err_t (*play_pause_cb)(bool want_play, void *ctx), void *play_pause_ctx,
                             void (*prev_cb)(void *ctx), void (*next_cb)(void *ctx), void *prev_next_ctx)
{
    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, BT_UI_WIDTH, CONTENT_H);
    lv_obj_center(root);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x102840), 0);

    lv_obj_t *cover_cont = lv_obj_create(root);
    lv_obj_remove_style_all(cover_cont);
    lv_obj_set_size(cover_cont, COVER_SIZE, COVER_SIZE);
    lv_obj_align(cover_cont, LV_ALIGN_LEFT_MID, MEDIA_LEFT_PAD, 0);
    lv_obj_clear_flag(cover_cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *cover_placeholder = lv_obj_create(cover_cont);
    lv_obj_remove_style_all(cover_placeholder);
    lv_obj_set_size(cover_placeholder, COVER_IMG_SIZE, COVER_IMG_SIZE);
    lv_obj_align(cover_placeholder, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(cover_placeholder, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(cover_placeholder, lv_color_hex(0x3d4f63), 0);
    lv_obj_set_style_radius(cover_placeholder, 8, 0);
    lv_obj_clear_flag(cover_placeholder, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *cover_type_label = lv_label_create(cover_placeholder);
    lv_label_set_text(cover_type_label, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_font(cover_type_label, BT_UI_FONT_COVER_ICON, 0);
    lv_obj_set_style_text_color(cover_type_label, lv_color_hex(0xa8b8c8), 0);
    lv_obj_center(cover_type_label);

    lv_obj_t *cover_type_desc_label = lv_label_create(cover_placeholder);
    lv_label_set_text(cover_type_desc_label, "Cover not received");
    lv_obj_set_style_text_font(cover_type_desc_label, BT_UI_FONT_SMALL, 0);
    lv_obj_set_style_text_color(cover_type_desc_label, lv_color_hex(0xa8b8c8), 0);
    lv_obj_align_to(cover_type_desc_label, cover_type_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);
#if CONFIG_GMF_EXAMPLE_AUDIO_TECH_LE
    lv_obj_add_flag(cover_type_desc_label, LV_OBJ_FLAG_HIDDEN);
#endif

    lv_obj_t *cover_type_img = lv_image_create(cover_placeholder);
    lv_obj_remove_style_all(cover_type_img);
    lv_obj_set_size(cover_type_img, 280, 187);
    lv_obj_center(cover_type_img);
    lv_obj_add_flag(cover_type_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(cover_type_img, LV_OBJ_FLAG_SCROLLABLE);

#if LVGL_VERSION_MAJOR >= 9
    lv_obj_t *cover_img = lv_image_create(cover_cont);
#else
    lv_obj_t *cover_img = lv_img_create(cover_cont);
#endif  /* LVGL_VERSION_MAJOR >= 9 */
    lv_obj_set_size(cover_img, COVER_IMG_SIZE, COVER_IMG_SIZE);
    lv_obj_align(cover_img, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(cover_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(cover_img, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *right_panel = lv_obj_create(root);
    lv_obj_remove_style_all(right_panel);
    lv_obj_set_size(right_panel, RIGHT_PANEL_W, COVER_SIZE);
    lv_obj_align(right_panel, LV_ALIGN_LEFT_MID, RIGHT_PANEL_X, 0);
    lv_obj_set_flex_flow(right_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(right_panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(right_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *name_cont = lv_obj_create(right_panel);
    lv_obj_remove_style_all(name_cont);
#if CONFIG_GMF_EXAMPLE_AUDIO_TECH_LE
    lv_obj_set_size(name_cont, RIGHT_PANEL_W, LE_TITLE_AREA_H);
#else
    lv_obj_set_size(name_cont, RIGHT_PANEL_W, NAME_AREA_H);
#endif  /* CONFIG_GMF_EXAMPLE_AUDIO_TECH_LE */
    lv_obj_clear_flag(name_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(name_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(name_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(name_cont, 16, 0);

    lv_obj_t *name_label = lv_label_create(name_cont);
    lv_label_set_text(name_label, "-----");
    lv_label_set_long_mode(name_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(name_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(name_label, lv_color_hex(0xe8eef4), 0);
    lv_obj_set_width(name_label, RIGHT_PANEL_W);
#if CONFIG_GMF_EXAMPLE_AUDIO_TECH_LE
    lv_obj_set_style_max_height(name_label, LE_TITLE_AREA_H, 0);
#else
    lv_obj_set_style_max_height(name_label, 160, 0);
#endif
    lv_obj_align(name_label, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_text_font(name_label, BT_UI_FONT_LARGE, 0);
    lv_obj_set_style_text_line_space(name_label, -4, 0);

    lv_obj_t *artist_label = lv_label_create(name_cont);
    lv_label_set_text(artist_label, "");
    lv_label_set_long_mode(artist_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(artist_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(artist_label, lv_color_hex(0xc9daea), 0);
    lv_obj_set_width(artist_label, RIGHT_PANEL_W);
    lv_obj_set_style_max_height(artist_label, 64, 0);
    /* Flex reflows after every metadata update, including multiline CJK names. */
    lv_obj_set_style_text_font(artist_label, BT_UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_line_space(artist_label, -4, 0);
#if CONFIG_GMF_EXAMPLE_AUDIO_TECH_LE
    lv_obj_add_flag(artist_label, LV_OBJ_FLAG_HIDDEN);
#endif  /* CONFIG_GMF_EXAMPLE_AUDIO_TECH_LE */

#if CONFIG_GMF_EXAMPLE_AUDIO_TECH_LE
    lv_obj_t *speaker_cont = lv_obj_create(right_panel);
    lv_obj_remove_style_all(speaker_cont);
    lv_obj_set_size(speaker_cont, RIGHT_PANEL_W, 0);
    lv_obj_set_flex_grow(speaker_cont, 1);
    lv_obj_clear_flag(speaker_cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *speaker_img = lv_image_create(speaker_cont);
    lv_obj_remove_style_all(speaker_img);
    lv_obj_set_size(speaker_img, SPEAKER_IMG_W, SPEAKER_IMG_H);
    lv_obj_clear_flag(speaker_img, LV_OBJ_FLAG_SCROLLABLE);
#if CONFIG_GMF_EXAMPLE_LE_LOCATION_FRONT_LEFT
    lv_image_set_src(speaker_img, speaker_img_for_location(ESP_BT_AUDIO_AUDIO_LOC_FRONT_LEFT));
#elif CONFIG_GMF_EXAMPLE_LE_LOCATION_FRONT_RIGHT
    lv_image_set_src(speaker_img, speaker_img_for_location(ESP_BT_AUDIO_AUDIO_LOC_FRONT_RIGHT));
#else
    lv_image_set_src(speaker_img, speaker_img_for_location(0));
#endif  /* CONFIG_GMF_EXAMPLE_LE_LOCATION_FRONT_LEFT */
    lv_obj_center(speaker_img);
#else
    lv_obj_t *spacer = lv_obj_create(right_panel);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_size(spacer, 0, 0);
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_clear_flag(spacer, LV_OBJ_FLAG_SCROLLABLE);
#endif  /* CONFIG_GMF_EXAMPLE_AUDIO_TECH_LE */

    lv_obj_t *btn_cont = lv_obj_create(right_panel);
    lv_obj_remove_style_all(btn_cont);
    lv_obj_set_size(btn_cont, RIGHT_PANEL_W, BTN_ROW_HEIGHT);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(btn_cont, LV_OBJ_FLAG_SCROLLABLE);

#if LVGL_VERSION_MAJOR >= 9
    lv_obj_t *btn_prev = lv_button_create(btn_cont);
#else
    lv_obj_t *btn_prev = lv_btn_create(btn_cont);
#endif  /* LVGL_VERSION_MAJOR >= 9 */
    lv_obj_set_size(btn_prev, BTN_W, BTN_H);
    lv_obj_set_style_shadow_width(btn_prev, 0, 0);
    lv_obj_t *lbl_prev = lv_label_create(btn_prev);
    lv_label_set_text(lbl_prev, LV_SYMBOL_PREV);
    lv_obj_set_style_text_font(lbl_prev, BT_UI_FONT_ICON, 0);
    lv_obj_center(lbl_prev);
    lv_obj_add_event_cb(btn_prev, btn_prev_click_cb, LV_EVENT_CLICKED, NULL);

#if LVGL_VERSION_MAJOR >= 9
    lv_obj_t *btn_play = lv_button_create(btn_cont);
#else
    lv_obj_t *btn_play = lv_btn_create(btn_cont);
#endif  /* LVGL_VERSION_MAJOR >= 9 */
    lv_obj_set_size(btn_play, BTN_W, BTN_H);
    lv_obj_set_style_shadow_width(btn_play, 0, 0);
    lv_obj_add_flag(btn_play, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_t *lbl_play = lv_label_create(btn_play);
    lv_label_set_text(lbl_play, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_font(lbl_play, BT_UI_FONT_ICON, 0);
    lv_obj_center(lbl_play);
    lv_obj_add_event_cb(btn_play, btn_play_pause_click_cb, LV_EVENT_VALUE_CHANGED, NULL);

#if LVGL_VERSION_MAJOR >= 9
    lv_obj_t *btn_next = lv_button_create(btn_cont);
#else
    lv_obj_t *btn_next = lv_btn_create(btn_cont);
#endif  /* LVGL_VERSION_MAJOR >= 9 */
    lv_obj_set_size(btn_next, BTN_W, BTN_H);
    lv_obj_set_style_shadow_width(btn_next, 0, 0);
    lv_obj_t *lbl_next = lv_label_create(btn_next);
    lv_label_set_text(lbl_next, LV_SYMBOL_NEXT);
    lv_obj_set_style_text_font(lbl_next, BT_UI_FONT_ICON, 0);
    lv_obj_center(lbl_next);
    lv_obj_add_event_cb(btn_next, btn_next_click_cb, LV_EVENT_CLICKED, NULL);

    static bt_ui_media_refs_t s_media_refs;
    s_media_refs.play_btn = btn_play;
    s_media_refs.title_label = name_label;
    s_media_refs.artist_label = artist_label;
    s_media_refs.cover_cont = cover_cont;
    s_media_refs.cover_img = cover_img;
    s_media_refs.cover_placeholder = cover_placeholder;
    s_media_refs.cover_type_label = cover_type_label;
    s_media_refs.cover_type_desc_label = cover_type_desc_label;
    s_media_refs.cover_type_img = cover_type_img;
    s_media_refs.cover_data = NULL;
    s_media_refs.cover_size = 0;
    s_media_refs.play_pause_cb = play_pause_cb;
    s_media_refs.play_pause_ctx = play_pause_ctx;
    s_media_refs.prev_cb = prev_cb;
    s_media_refs.next_cb = next_cb;
    s_media_refs.prev_next_ctx = prev_next_ctx;
#if CONFIG_GMF_EXAMPLE_AUDIO_TECH_LE
    s_media_refs.speaker_img = speaker_img;
#endif  /* CONFIG_GMF_EXAMPLE_AUDIO_TECH_LE */
    lv_obj_set_user_data(root, &s_media_refs);

    return root;
}

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "platform.h"
#include "bt_ui.h"
#include "bt_debug.h"
#include "bt_metadata_text.h"
#include "src/draw/snapshot/lv_snapshot.h"
static bool requested;
static unsigned requests;
static esp_err_t result;
static bt_power_status_t power_state;
static void power_status(bt_power_status_t *out, void *ctx) { (void)ctx; *out = power_state; }
static esp_err_t power_request(bool enabled, void *ctx)
{
    (void)ctx; requested = enabled; ++requests;
    if (result == ESP_OK) power_state.state = enabled ? BT_POWER_STARTING : BT_POWER_STOPPING;
    return result;
}
static esp_err_t accepted(bool play, void *ctx) { (void)ctx; requested = play; ++requests; return result; }
static unsigned previous_calls, next_calls, dial_calls, answer_calls, end_calls;
static int callback_context;
static char dial_number[32];
static void previous(void *ctx) { assert(ctx == &callback_context); ++previous_calls; }
static void next(void *ctx) { assert(ctx == &callback_context); ++next_calls; }
static void dial(const char *number, void *ctx)
{
    assert(ctx == &callback_context); ++dial_calls;
    snprintf(dial_number, sizeof(dial_number), "%s", number);
}
static void answer(void *ctx) { assert(ctx == &callback_context); ++answer_calls; }
static void end_call(void *ctx) { assert(ctx == &callback_context); ++end_calls; }
/* Header/SOS envelope only; the fake decoder isolates widget/ownership/locking. */
static const uint8_t fixture[] = {0xff, 0xd8, 0xff, 0xc0, 0, 11, 8, 0, 32, 0, 32, 1, 1, 0x11, 0,
    0xff, 0xda, 0, 8, 1, 1, 0, 0, 0x3f, 0, 0, 0xff, 0xd9};
static lv_obj_t *find_cover(lv_obj_t *root)
{
    if (lv_obj_check_type(root, &lv_image_class) && lv_obj_get_width(root) == 340) return root;
    for (unsigned i = 0; i < lv_obj_get_child_count(root); ++i) {
        lv_obj_t *found = find_cover(lv_obj_get_child(root, i)); if (found) return found;
    }
    return NULL;
}
static lv_obj_t *find_label(lv_obj_t *root, const char *text)
{
    if (lv_obj_check_type(root, &lv_label_class) && !strcmp(lv_label_get_text(root), text)) return root;
    for (unsigned i = 0; i < lv_obj_get_child_count(root); ++i) {
        lv_obj_t *found = find_label(lv_obj_get_child(root, i), text); if (found) return found;
    }
    return NULL;
}
static lv_obj_t *find_text(lv_obj_t *root, const char *text)
{
    if (lv_obj_check_type(root, &lv_label_class) && strstr(lv_label_get_text(root), text)) return root;
    for (unsigned i = 0; i < lv_obj_get_child_count(root); ++i) {
        lv_obj_t *found = find_text(lv_obj_get_child(root, i), text); if (found) return found;
    }
    return NULL;
}
static lv_obj_t *find_switch(lv_obj_t *parent)
{
    for (unsigned i = 0; i < lv_obj_get_child_count(parent); ++i) {
        lv_obj_t *child = lv_obj_get_child(parent, i);
        if (lv_obj_check_type(child, &lv_switch_class)) return child;
    }
    assert(!"switch not found"); return NULL;
}
static lv_obj_t *find_volume_bar(lv_obj_t *root)
{
    if (lv_obj_get_width(root) == 6 && lv_obj_get_height(root) == 96) return root;
    for (unsigned i = 0; i < lv_obj_get_child_count(root); ++i) {
        lv_obj_t *found = find_volume_bar(lv_obj_get_child(root, i)); if (found) return found;
    }
    return NULL;
}
static void snapshot(lv_obj_t *screen, const char *path)
{
    lv_draw_buf_t *buf = lv_snapshot_take(screen, LV_COLOR_FORMAT_RGB888); assert(buf);
    FILE *file = fopen(path, "wb"); assert(file);
    fprintf(file, "P6\n%u %u\n255\n", buf->header.w, buf->header.h);
    for (unsigned y = 0; y < buf->header.h; ++y) {
        const uint8_t *row = buf->data + y * buf->header.stride;
        for (unsigned x = 0; x < buf->header.w; ++x) {
            uint8_t rgb[] = {row[x * 3 + 2], row[x * 3 + 1], row[x * 3]};
            assert(fwrite(rgb, 1, 3, file) == 3);
        }
    }
    assert(fclose(file) == 0); lv_draw_buf_destroy(buf);
}
static double linear(unsigned value)
{
    double x = value / 255.0; return x <= 0.04045 ? x / 12.92 : pow((x + .055) / 1.055, 2.4);
}
static double light(lv_color_t c) { return .2126 * linear(c.red) + .7152 * linear(c.green) + .0722 * linear(c.blue); }
int main(int argc, char **argv)
{
    assert(argc == 2);
    setbuf(stdout, NULL);
    lv_init();
    lv_display_t *display = lv_display_create(800, 480);
    assert(display);
    bt_ui_config_t cfg = {.play_pause_cb = accepted, .prev_cb = previous, .next_cb = next,
        .prev_next_ctx = &callback_context};
    if (strncmp(argv[1], "power_", 6) == 0) {
        bool off = !strcmp(argv[1], "power_on") || !strcmp(argv[1], "power_error");
        power_state = (bt_power_status_t){.state = off ? BT_POWER_OFF : BT_POWER_ON, .controller_enabled = !off};
        if (!strcmp(argv[1], "power_error")) power_state.state = BT_POWER_ERROR;
        cfg.power_request_cb = power_request;
        cfg.power_status_cb = power_status;
    }
    bt_ui_t *ui = bt_ui_create("HOST_UI", &cfg); assert(ui);
    lv_obj_t *screen = lv_screen_active();
    lv_obj_update_layout(screen);
    if (!strncmp(argv[1], "power_", 6)) {
        lv_obj_t *label = find_label(screen, "Bluetooth power"); assert(label);
        lv_obj_t *page = lv_obj_get_parent(label), *control = NULL;
        for (unsigned i = 0; i < lv_obj_get_child_count(page); ++i) {
            lv_obj_t *child = lv_obj_get_child(page, i);
            if (lv_obj_check_type(child, &lv_switch_class)) control = child;
        }
        assert(control && !lv_obj_has_state(control, LV_STATE_DISABLED));
        bool turn_on = !strcmp(argv[1], "power_on");
        bool was_enabled = power_state.controller_enabled;
        result = !strcmp(argv[1], "power_reject") ? ESP_FAIL : ESP_OK;
        if (!strcmp(argv[1], "power_error")) {
            lv_obj_t *retry = find_label(page, "Retry safe OFF"); assert(retry);
            assert(!lv_obj_has_flag(lv_obj_get_parent(retry), LV_OBJ_FLAG_HIDDEN));
            lv_obj_send_event(lv_obj_get_parent(retry), LV_EVENT_CLICKED, NULL);
        } else {
            if (turn_on) lv_obj_add_state(control, LV_STATE_CHECKED);
            else lv_obj_remove_state(control, LV_STATE_CHECKED);
            lv_obj_send_event(control, LV_EVENT_VALUE_CHANGED, NULL);
        }
        assert(requests == 1 && requested == turn_on);
        assert(lv_obj_has_state(control, LV_STATE_CHECKED) == was_enabled);
        assert(lv_obj_has_state(control, LV_STATE_DISABLED) == (result == ESP_OK));
        power_state.state = turn_on ? BT_POWER_ON : BT_POWER_OFF;
        power_state.controller_enabled = turn_on;
        lv_tick_inc(501); lv_timer_handler();
        assert(lv_obj_has_state(control, LV_STATE_CHECKED) == turn_on);
        assert(!lv_obj_has_state(control, LV_STATE_DISABLED));
    } else if (!strcmp(argv[1], "power")) {
        lv_obj_t *label = find_label(screen, "Bluetooth power"); assert(label);
        lv_obj_t *page = lv_obj_get_parent(label);
        bool found = false;
        for (unsigned i = 0; i < lv_obj_get_child_count(page); ++i) {
            lv_obj_t *child = lv_obj_get_child(page, i);
            if (lv_obj_check_type(child, &lv_switch_class)) {
                found = true;
                /* A UI with no backend must not offer a pretend power switch. */
                assert(lv_obj_has_state(child, LV_STATE_DISABLED));
            }
        }
        assert(found);
    } else if (!strcmp(argv[1], "contrast")) {
        lv_obj_t *label = find_label(screen, "Application / API"); assert(label);
        lv_obj_t *page = lv_obj_get_parent(lv_obj_get_parent(lv_obj_get_parent(label)));
        assert(lv_obj_get_style_bg_opa(page, LV_PART_MAIN) == LV_OPA_COVER);
        lv_obj_t *row = lv_obj_get_parent(label);
        double text = light(lv_obj_get_style_text_color(label, LV_PART_MAIN));
        double bg = light(lv_obj_get_style_bg_color(row, LV_PART_MAIN));
        double ratio = (fmax(text, bg) + .05) / (fmin(text, bg) + .05);
        printf("Actual Debug card text/background contrast: %.2f:1\n", ratio);
        assert(ratio >= 4.5);
    } else if (!strcmp(argv[1], "play") || !strcmp(argv[1], "play_reject")) {
        lv_obj_t *play = find_label(screen, LV_SYMBOL_PLAY); assert(play);
        lv_obj_t *button = lv_obj_get_parent(play);
        result = !strcmp(argv[1], "play_reject") ? ESP_FAIL : ESP_OK;
        if (lv_obj_has_flag(button, LV_OBJ_FLAG_CHECKABLE)) {
            lv_obj_add_state(button, LV_STATE_CHECKED);
            lv_obj_send_event(button, LV_EVENT_VALUE_CHANGED, NULL);
        } else lv_obj_send_event(button, LV_EVENT_CLICKED, NULL);
        assert(requests == 1 && requested);
        assert(!strcmp(lv_label_get_text(play), result == ESP_OK ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY));
        assert(lv_obj_has_state(button, LV_STATE_CHECKED) == (result == ESP_OK));
        bt_ui_update_playback_status(ui, ESP_BT_AUDIO_PLAYBACK_STATUS_PAUSED);
        assert(!strcmp(lv_label_get_text(play), LV_SYMBOL_PLAY));
    } else if (!strcmp(argv[1], "media_controls")) {
        lv_obj_send_event(lv_obj_get_parent(find_label(screen, LV_SYMBOL_PREV)), LV_EVENT_CLICKED, NULL);
        lv_obj_send_event(lv_obj_get_parent(find_label(screen, LV_SYMBOL_NEXT)), LV_EVENT_CLICKED, NULL);
        assert(previous_calls == 1 && next_calls == 1 && requests == 0);
    } else if (!strcmp(argv[1], "debug_controls")) {
        int index = bt_debug_module_find("A2D"); assert(index >= 0);
        int other = bt_debug_module_find("AVRC"); assert(other >= 0);
        lv_obj_t *row = lv_obj_get_parent(find_label(screen, bt_debug_module_at(index)->title));
        lv_obj_t *control = find_switch(row);
        lv_obj_add_state(control, LV_STATE_CHECKED);
        lv_obj_send_event(control, LV_EVENT_VALUE_CHANGED, NULL);
        assert(bt_debug_enabled(index) && !bt_debug_enabled(other));
        bt_debug_set(index, false); bt_debug_set(other, true);
        lv_tick_inc(501); lv_timer_handler();
        assert(!lv_obj_has_state(control, LV_STATE_CHECKED));
        lv_obj_t *other_control = find_switch(lv_obj_get_parent(find_label(screen, bt_debug_module_at(other)->title)));
        assert(lv_obj_has_state(other_control, LV_STATE_CHECKED));
        lv_obj_t *page = lv_obj_get_parent(find_label(screen, "Save ESP text logs to SD (serial stays on)"));
        lv_obj_t *sd = find_switch(page);
        lv_obj_add_state(sd, LV_STATE_CHECKED); lv_obj_send_event(sd, LV_EVENT_VALUE_CHANGED, NULL);
        bt_debug_status_t status; bt_debug_get_status(&status); assert(status.sd_requested);
        lv_obj_send_event(lv_obj_get_parent(find_label(screen, "All DEBUG off")), LV_EVENT_CLICKED, NULL);
        lv_tick_inc(501); lv_timer_handler();
        assert(!bt_debug_enabled(other) && !lv_obj_has_state(other_control, LV_STATE_CHECKED));
        bt_debug_get_status(&status); assert(status.sd_requested && lv_obj_has_state(sd, LV_STATE_CHECKED));
        bt_debug_sd_set(false); lv_tick_inc(501); lv_timer_handler();
        assert(!lv_obj_has_state(sd, LV_STATE_CHECKED));
    } else if (!strcmp(argv[1], "debug_snapshot")) {
        uint8_t addr[6] = {1, 2, 3, 4, 5, 6};
        bt_ui_set_connected(ui, true, addr);
        bt_ui_update_stream_type(ui, &callback_context, ESP_BT_AUDIO_STREAM_STATE_STARTED);
        bt_ui_update_playback_status(ui, ESP_BT_AUDIO_PLAYBACK_STATUS_PLAYING);
        bt_ui_update_volume(ui, 170); assert(bt_ui_get_volume(ui) == 100);
        lv_tick_inc(501); lv_timer_handler();
        assert(find_text(screen, "Link: CONNECTED | Stream: STARTED | Playback: PLAYING"));
        bt_ui_set_connected(ui, false, addr); lv_tick_inc(501); lv_timer_handler();
        assert(find_text(screen, "Link: DISCONNECTED | Stream: UNKNOWN | Playback: STOPPED"));
    } else if (!strcmp(argv[1], "volume_timer")) {
        lv_obj_t *bar = find_volume_bar(screen); assert(bar);
        bt_ui_volume_bar_set_level(bar, -10); lv_obj_update_layout(screen);
        assert(lv_obj_get_height(lv_obj_get_child(bar, 0)) == 0);
        bt_ui_volume_bar_set_level(bar, 130); lv_obj_update_layout(screen);
        assert(lv_obj_get_height(lv_obj_get_child(bar, 0)) == lv_obj_get_height(bar));
        lv_tick_inc(700); lv_timer_handler();
        assert(!lv_obj_has_flag(bar, LV_OBJ_FLAG_HIDDEN));
        bt_ui_volume_bar_set_level(bar, 50);
        lv_tick_inc(700); lv_timer_handler();
        assert(!lv_obj_has_flag(bar, LV_OBJ_FLAG_HIDDEN));
        lv_tick_inc(301); lv_timer_handler();
        assert(lv_obj_has_flag(bar, LV_OBJ_FLAG_HIDDEN));
    } else if (!strcmp(argv[1], "dialer_controls")) {
        lv_obj_t *page = bt_ui_dialer_create(screen, dial, &callback_context,
            end_call, &callback_context, answer, &callback_context); assert(page);
        lv_obj_send_event(lv_obj_get_parent(find_label(page, "1")), LV_EVENT_CLICKED, NULL);
        lv_obj_send_event(lv_obj_get_parent(find_label(page, "2")), LV_EVENT_CLICKED, NULL);
        assert(find_label(page, "12"));
        lv_obj_send_event(lv_obj_get_parent(find_label(page, LV_SYMBOL_BACKSPACE)), LV_EVENT_CLICKED, NULL);
        assert(find_label(page, "1"));
        lv_obj_t *call = lv_obj_get_parent(find_label(page, LV_SYMBOL_CALL));
        lv_obj_send_event(call, LV_EVENT_CLICKED, NULL);
        assert(dial_calls == 1 && !strcmp(dial_number, "1"));
        bt_ui_dialer_set_call_state(page, ESP_BT_AUDIO_CALL_STATE_INCOMING, "123");
        assert(find_label(page, "Incoming  123"));
        lv_obj_t *answer_button = lv_obj_get_parent(find_label(page, LV_SYMBOL_CALL));
        assert(!lv_obj_has_flag(answer_button, LV_OBJ_FLAG_HIDDEN));
        lv_obj_send_event(answer_button, LV_EVENT_CLICKED, NULL); assert(answer_calls == 1);
        lv_obj_send_event(call, LV_EVENT_CLICKED, NULL); assert(end_calls == 1);
        bt_ui_dialer_set_call_state(page, 0, NULL); assert(find_label(page, "Enter Number"));
        assert(lv_obj_has_flag(answer_button, LV_OBJ_FLAG_HIDDEN));
    } else if (!strcmp(argv[1], "fonts")) {
        lv_obj_t *title = find_label(screen, "-----"); assert(title);
        const lv_font_t *font = lv_obj_get_style_text_font(title, LV_PART_MAIN);
        const uint32_t chars[] = {0x4e2d, 0x3042, 0x30a2, 0xd55c, 0xae00, 0x3131};
        for (unsigned i = 0; i < sizeof(chars) / sizeof(chars[0]); ++i) {
            lv_font_glyph_dsc_t glyph;
            bool found = lv_font_get_glyph_dsc(font, &glyph, chars[i], 0);
            printf("U+%04x font glyph: %s\n", chars[i], found && !glyph.is_placeholder ? "present" : "MISSING");
            assert(found && !glyph.is_placeholder);
        }
        for (uint32_t ch = 0xac00; ch <= 0xd7a3; ++ch) {
            lv_font_glyph_dsc_t glyph;
            assert(lv_font_get_glyph_dsc(font, &glyph, ch, 0) && !glyph.is_placeholder);
        }
        puts("All 11172 modern Hangul syllables present");
    } else if (!strcmp(argv[1], "cover_clear")) {
        bt_ui_post_cover(ui, NULL, 0);
        assert(host_pending_covers != 0);
        host_run_cover_task();
    } else if (!strcmp(argv[1], "decode_lock")) {
        bt_ui_post_cover(ui, fixture, sizeof(fixture));
        host_run_cover_task();
        assert(!host_decode_locked);
        assert(host_decode_calls == 1);
        assert(!lv_obj_has_flag(find_cover(screen), LV_OBJ_FLAG_HIDDEN));
    } else if (!strcmp(argv[1], "cover_stale")) {
        bt_ui_post_cover(ui, fixture, sizeof(fixture));
        bt_ui_post_cover(ui, NULL, 0);
        host_run_cover_task();
        assert(host_decode_calls == 0);
        assert(lv_obj_has_flag(find_cover(screen), LV_OBJ_FLAG_HIDDEN));
    } else if (!strcmp(argv[1], "cover_decode_stale")) {
        host_during_decode = ui;
        bt_ui_post_cover(ui, fixture, sizeof(fixture));
        host_run_cover_task();
        assert(host_decode_calls == 1);
        assert(lv_obj_has_flag(find_cover(screen), LV_OBJ_FLAG_HIDDEN));
    } else if (!strcmp(argv[1], "cover_failure")) {
        bt_ui_post_cover(ui, fixture, sizeof(fixture)); host_run_cover_task();
        host_decode_fail = true;
        bt_ui_post_cover(ui, fixture, sizeof(fixture)); host_run_cover_task();
        assert(lv_obj_has_flag(find_cover(screen), LV_OBJ_FLAG_HIDDEN));
    } else if (!strcmp(argv[1], "cover_bounds")) {
        bt_ui_post_cover(ui, fixture, 512 * 1024 + 1);
        assert(host_pending_covers == 0);
        for (size_t n = 1; n < 15; ++n) {
            bt_ui_post_cover(ui, fixture, n); host_run_cover_task();
        }
        uint8_t invalid[sizeof(fixture)]; memcpy(invalid, fixture, sizeof(invalid));
        invalid[8] = 0xff; invalid[9] = 0xff;
        bt_ui_post_cover(ui, invalid, sizeof(invalid)); host_run_cover_task();
        assert(host_decode_calls == 0);
    } else if (!strcmp(argv[1], "cover_skip")) {
        uint8_t malformed[19]; memcpy(malformed, fixture, 15);
        memcpy(malformed + 15, "\xff\xe1\xff\xff", 4);
        bt_ui_post_cover(ui, malformed, sizeof(malformed)); host_run_cover_task();
        assert(host_decode_calls == 0);
    } else if (!strcmp(argv[1], "cover_queue")) {
        for (unsigned i = 0; i < 6; ++i) bt_ui_post_cover(ui, fixture, sizeof(fixture));
        assert(host_pending_covers == 2);
        host_run_cover_task();
        assert(host_decode_calls == 2);
        bt_ui_post_cover(ui, NULL, 0); host_run_cover_task();
        assert(lv_obj_has_flag(find_cover(screen), LV_OBJ_FLAG_HIDDEN));
    } else if (!strcmp(argv[1], "disconnect")) {
        bt_ui_update_track(ui, "日本語 한글", "Artist");
        bt_ui_post_cover(ui, fixture, sizeof(fixture)); host_run_cover_task();
        uint8_t addr[6] = {0}; bt_ui_set_connected(ui, false, addr); host_run_cover_task();
        assert(find_label(screen, "No music playing"));
        assert(!find_label(screen, "Artist"));
        assert(lv_obj_has_flag(find_cover(screen), LV_OBJ_FLAG_HIDDEN));
    } else if (!strcmp(argv[1], "utf8")) {
        char text[8];
        bt_metadata_text_copy(text, sizeof(text), (const uint8_t *)"a한글b", 8); assert(!strcmp(text, "a한글"));
        bt_metadata_text_copy(text, 4, (const uint8_t *)"aあ", 4); assert(!strcmp(text, "a"));
        bt_metadata_text_copy(text, 5, (const uint8_t *)"aあ", 4); assert(!strcmp(text, "aあ"));
        bt_metadata_text_copy(text, sizeof(text), NULL, 100); assert(!*text);
        const uint8_t bad[][4] = {{0x80}, {0xe3, 0x81}, {0xed, 0xa0, 0x80}, {0xf4, 0x90, 0x80, 0x80}, {0xc0, 0x80}, {0xe0, 0x80, 0x80}};
        for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
            bt_metadata_text_copy(text, sizeof(text), bad[i], sizeof(bad[i])); assert(!*text);
        }
    } else if (!strcmp(argv[1], "media_layout") || !strcmp(argv[1], "media_long") || !strcmp(argv[1], "render_media")) {
        const char *title = "日本語の歌\nひらがな・カタカナ\n한국어 한글";
        bt_ui_update_track(ui, title, "中文 / 日本語 / 한국어");
        lv_obj_t *label = find_label(screen, title); assert(label);
        lv_obj_t *artist = find_label(screen, "中文 / 日本語 / 한국어"); assert(artist);
        lv_obj_t *tab = lv_obj_get_parent(lv_obj_get_parent(lv_obj_get_parent(label)));
        lv_obj_t *tabview = lv_obj_get_parent(lv_obj_get_parent(lv_obj_get_parent(tab)));
        lv_tabview_set_act(tabview, 0, LV_ANIM_OFF);
        lv_obj_update_layout(screen);
        lv_area_t title_area, artist_area;
        lv_obj_get_coords(label, &title_area); lv_obj_get_coords(artist, &artist_area);
        if (!strcmp(argv[1], "render_media")) snapshot(screen, "/tmp/esp32-media-host.ppm");
        assert(title_area.y2 < artist_area.y1);
        if (!strcmp(argv[1], "media_long")) {
            char long_title[211];
            for (unsigned i = 0; i < 70; ++i) memcpy(long_title + i * 3, "中", 3);
            long_title[210] = 0;
            bt_ui_update_track(ui, long_title, "Artist"); lv_obj_update_layout(screen);
            lv_area_t area; lv_obj_get_coords(lv_obj_get_parent(label), &area);
            lv_obj_get_coords(label, &title_area); lv_obj_get_coords(artist, &artist_area);
            assert(title_area.y1 >= area.y1 && title_area.y2 < artist_area.y1 && artist_area.y2 <= area.y2);
        }
    } else if (!strcmp(argv[1], "render_debug")) {
        snapshot(screen, "/tmp/esp32-debug-host.ppm");
    } else if (!strcmp(argv[1], "antitear")) {
        assert(bt_ui_init() == ESP_OK);
        assert(host_display.buffer_size == 800 * 480);
        assert(host_display.flags.direct_mode && !host_display.flags.full_refresh && !host_display.double_buffer);
        assert(host_rgb.flags.avoid_tearing && !host_rgb.flags.bb_mode);
    } else assert(!"unknown test");
    puts("UI regression PASS");
}

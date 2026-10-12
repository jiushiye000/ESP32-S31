#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "platform.h"
#include "bt_ui.h"
static bool requested;
static unsigned requests;
static void accepted(bool play, void *ctx) { (void)ctx; requested = play; ++requests; }
static lv_obj_t *find_label(lv_obj_t *root, const char *text)
{
    if (lv_obj_check_type(root, &lv_label_class) && !strcmp(lv_label_get_text(root), text)) return root;
    for (unsigned i = 0; i < lv_obj_get_child_count(root); ++i) {
        lv_obj_t *found = find_label(lv_obj_get_child(root, i), text); if (found) return found;
    }
    return NULL;
}
static double linear(unsigned value)
{
    double x = value / 255.0; return x <= 0.04045 ? x / 12.92 : pow((x + .055) / 1.055, 2.4);
}
static double light(lv_color_t c) { return .2126 * linear(c.red) + .7152 * linear(c.green) + .0722 * linear(c.blue); }
int main(int argc, char **argv)
{
    assert(argc == 2);
    lv_init();
    lv_display_t *display = lv_display_create(800, 480);
    assert(display);
    bt_ui_config_t cfg = {.play_pause_cb = accepted};
    bt_ui_t *ui = bt_ui_create("HOST_UI", &cfg); assert(ui);
    lv_obj_t *screen = lv_screen_active();
    lv_obj_update_layout(screen);
    if (!strcmp(argv[1], "contrast")) {
        lv_obj_t *label = find_label(screen, "Application / API"); assert(label);
        lv_obj_t *row = lv_obj_get_parent(label);
        double text = light(lv_obj_get_style_text_color(label, LV_PART_MAIN));
        double bg = light(lv_obj_get_style_bg_color(row, LV_PART_MAIN));
        double ratio = (fmax(text, bg) + .05) / (fmin(text, bg) + .05);
        printf("Actual Debug card text/background contrast: %.2f:1\n", ratio);
        assert(ratio >= 4.5);
    } else if (!strcmp(argv[1], "play")) {
        lv_obj_t *play = find_label(screen, LV_SYMBOL_PLAY); assert(play);
        lv_obj_t *button = lv_obj_get_parent(play);
        if (lv_obj_has_flag(button, LV_OBJ_FLAG_CHECKABLE)) {
            lv_obj_add_state(button, LV_STATE_CHECKED);
            lv_obj_send_event(button, LV_EVENT_VALUE_CHANGED, NULL);
        } else lv_obj_send_event(button, LV_EVENT_CLICKED, NULL);
        assert(requests == 1 && requested);
        assert(!strcmp(lv_label_get_text(play), LV_SYMBOL_PAUSE));
        bt_ui_update_playback_status(ui, ESP_BT_AUDIO_PLAYBACK_STATUS_PAUSED);
        assert(!strcmp(lv_label_get_text(play), LV_SYMBOL_PLAY));
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
    } else if (!strcmp(argv[1], "cover_clear")) {
        bt_ui_post_cover(ui, NULL, 0);
        assert(host_pending_covers != 0);
        host_run_cover_task();
    } else if (!strcmp(argv[1], "decode_lock")) {
        const uint8_t fixture[] = {1, 2, 3};
        bt_ui_post_cover(ui, fixture, sizeof(fixture));
        host_run_cover_task();
        assert(!host_decode_locked);
    } else assert(!"unknown test");
    puts("UI regression PASS");
}

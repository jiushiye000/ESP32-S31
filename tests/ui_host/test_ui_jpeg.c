#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform.h"
#include "bt_ui.h"

static lv_obj_t *find_cover(lv_obj_t *root)
{
    if (lv_obj_check_type(root, &lv_image_class) && lv_obj_get_width(root) == 340) return root;
    for (unsigned i = 0; i < lv_obj_get_child_count(root); ++i) {
        lv_obj_t *found = find_cover(lv_obj_get_child(root, i)); if (found) return found;
    }
    return NULL;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    FILE *file = fopen(argv[1], "rb"); assert(file);
    assert(fseek(file, 0, SEEK_END) == 0); long length = ftell(file); assert(length > 0);
    rewind(file); uint8_t *jpeg = malloc((size_t)length); assert(jpeg);
    assert(fread(jpeg, 1, (size_t)length, file) == (size_t)length); fclose(file);
    lv_init(); assert(lv_display_create(800, 480));
    bt_ui_t *ui = bt_ui_create("REAL_JPEG", NULL); assert(ui);
    lv_obj_update_layout(lv_screen_active());
    lv_obj_t *cover = find_cover(lv_screen_active()); assert(cover);
    bt_ui_post_cover(ui, jpeg, (size_t)length); host_run_cover_task();
    assert(!lv_obj_has_flag(cover, LV_OBJ_FLAG_HIDDEN));
    const lv_image_dsc_t *image = lv_image_get_src(cover);
    assert(image->header.w == 46 && image->header.h == 46 && image->data_size == 46 * 46 * 2);
    /* Every truncation of the actual official logo fixture must fail safely.
     * Some complete scans without EOI may be accepted by the official decoder. */
    for (size_t size = 1; size < (size_t)length; ++size) {
        bt_ui_post_cover(ui, jpeg, size); host_run_cover_task();
    }
    const uint8_t bad[] = {0xff, 0xd8, 0xff, 0xc0, 0, 11, 8, 0, 32, 0, 32, 1, 1, 0x11, 0,
        0xff, 0xe1, 0xff, 0xff};
    bt_ui_post_cover(ui, bad, sizeof(bad)); host_run_cover_task();
    assert(lv_obj_has_flag(cover, LV_OBJ_FLAG_HIDDEN));
    bt_ui_post_cover(ui, jpeg, (size_t)length); host_run_cover_task();
    assert(!lv_obj_has_flag(cover, LV_OBJ_FLAG_HIDDEN));
    bt_ui_post_cover(ui, NULL, 0); host_run_cover_task();
    free(jpeg);
    puts("Real esp_jpeg wrapper / external tjpgd: decode, all truncations, malformed skip, recovery PASS");
}

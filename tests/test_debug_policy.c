#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "bt_debug_policy.h"

typedef struct { const char *tag; int level; } tag_level_t;
static tag_level_t observed[128];
static size_t observed_count;

static void apply(const char *tag, int level, void *ctx)
{
    assert(ctx == observed);
    for (size_t i = 0; i < observed_count; ++i) {
        if (strcmp(observed[i].tag, tag) == 0) {
            observed[i].level = level;
            return;
        }
    }
    assert(observed_count < 128);
    observed[observed_count++] = (tag_level_t){tag, level};
}

static int level(const char *tag)
{
    for (size_t i = 0; i < observed_count; ++i) {
        if (strcmp(observed[i].tag, tag) == 0) return observed[i].level;
    }
    assert(!"tag not applied");
    return -1;
}

int main(void)
{
    bt_debug_policy_t policy;
    uint32_t caps = BT_DEBUG_CAP_DEBUG | BT_DEBUG_CAP_CLASSIC | BT_DEBUG_CAP_UI;
    assert(!bt_debug_policy_init(NULL, caps, apply, observed));
    assert(!bt_debug_policy_init(&policy, caps, NULL, observed));
    assert(bt_debug_policy_init(&policy, caps, apply, observed));
    assert(bt_debug_module_count() > 10 && bt_debug_module_count() < 32);
    assert(bt_debug_module_at(bt_debug_module_count()) == NULL);
    assert(bt_debug_module_find(NULL) == -1);
    assert(bt_debug_module_find("missing") == -1);
    int a2d = bt_debug_module_find("A2D");
    int avrc = bt_debug_module_find("AVRC");
    int packets = bt_debug_module_find("A2D_PACKETS");
    int hfp = bt_debug_module_find("HFP");
    int ble = bt_debug_module_find("BLE");
    assert(a2d >= 0 && avrc >= 0 && packets >= 0 && hfp >= 0 && ble >= 0);
    assert(!bt_debug_policy_supported(&policy, hfp));
    assert(!bt_debug_policy_supported(&policy, ble));
    assert(!bt_debug_policy_set(&policy, hfp, true));
    assert(!bt_debug_policy_set(&policy, bt_debug_module_count(), true));
    assert(!bt_debug_policy_set(NULL, 0, true));
    assert(!bt_debug_policy_enabled(NULL, 0));
    assert(!bt_debug_policy_supported(NULL, 0));
    assert(!bt_debug_policy_enabled(&policy, bt_debug_module_count()));
    assert(bt_debug_policy_set(&policy, a2d, true));
    assert(bt_debug_policy_enabled(&policy, a2d));
    assert(level("BT_A2D") == 4);
    assert(level("BT_AVRC") == 2); /* independent profile */
    assert(level("BT_AUD_A2D_SINK") == 3); /* packets remain opt-in */
    assert(bt_debug_policy_set(&policy, avrc, true));
    assert(level("BT_AUD_AVRC_CT") == 4 && level("BT_AUD_AVRC_TG") == 4);
    assert(bt_debug_policy_set(&policy, a2d, false));
    assert(level("BT_A2D") == 2 && level("BT_AVRC") == 4);
    assert(bt_debug_policy_set(&policy, packets, true));
    assert(level("BT_AUD_A2D_SINK") == 4);
    uint32_t saved = policy.enabled_mask;
    bt_debug_policy_restore(&policy, UINT32_MAX);
    assert(!bt_debug_policy_enabled(&policy, hfp));
    assert(!bt_debug_policy_enabled(&policy, ble));
    for (size_t i = 0; i < bt_debug_module_count(); ++i) {
        const bt_debug_module_t *module = bt_debug_module_at(i);
        assert(bt_debug_module_find(module->key) == (int)i);
        assert(module->title && module->hint && module->tags[0]);
        if (bt_debug_policy_supported(&policy, i)) {
            assert(bt_debug_policy_set(&policy, i, false));
            assert(bt_debug_policy_set(&policy, i, true));
        }
    }
    bt_debug_policy_restore(&policy, saved);
    assert(policy.enabled_mask == saved);
    bt_debug_policy_restore(&policy, 0);
    assert(level("BT_AVRC") == 2 && level("BT_AUD_AVRC_CT") == 3);
    assert(bt_debug_policy_init(&policy, UINT32_MAX, apply, observed));
    assert(bt_debug_policy_set(&policy, hfp, true));
    assert(bt_debug_policy_set(&policy, ble, true));
    assert(level("BT_RFCOMM") == 4 && level("BT_GATT") == 4);
    assert(bt_debug_policy_init(&policy, 0, apply, observed));
    assert(!bt_debug_policy_set(&policy, a2d, true));
    bt_debug_policy_restore(NULL, 0);
    puts("debug policy: independent modules, baselines, packet opt-in and capability gates PASS");
}

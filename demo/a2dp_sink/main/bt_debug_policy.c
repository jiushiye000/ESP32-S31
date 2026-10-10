#include "bt_debug_policy.h"
#include <string.h>

#define CLASSIC (BT_DEBUG_CAP_DEBUG | BT_DEBUG_CAP_CLASSIC)
#define APP_TAG(tag) {tag, 3}
#define STACK_TAG(tag) {tag, 2}

/* Quec-style layer names, mapped to this firmware's real ESP tags. */
static const bt_debug_module_t modules[] = {
    {"API", "Application / API", "Demo callbacks and public audio API", BT_DEBUG_CAP_DEBUG,
        {APP_TAG("BT_AUD_EXAMPLE"), APP_TAG("BT_AUDIO"), APP_TAG("BT_AUD_EVT")}},
    {"GAP", "GAP / discovery", "Discovery, pairing and host events", CLASSIC,
        {STACK_TAG("BT_GAP"), APP_TAG("BT_AUD_HOST"), APP_TAG("BT_AUD_CLASSIC")}},
    {"BTM", "BTM / link manager", "ACL, security and link management", CLASSIC,
        {STACK_TAG("BT_BTM")}},
    {"HCI", "HCI", "Host commands and controller events; not btsnoop", CLASSIC,
        {STACK_TAG("BT_HCI")}},
    {"L2CAP", "L2CAP", "Channels and signalling", CLASSIC, {STACK_TAG("BT_L2CAP")}},
    {"SDP", "SDP", "Service discovery", CLASSIC, {STACK_TAG("BT_SDP")}},
    {"A2D", "A2DP", "Profile traces; Sink packet tag stays INFO", CLASSIC,
        {STACK_TAG("BT_A2D"), APP_TAG("BT_AUD_A2D_SRC")}},
    {"AVDT", "AVDTP", "Audio transport and stream negotiation", CLASSIC, {STACK_TAG("BT_AVDT")}},
    {"AVCT", "AVCTP", "AVRCP transport and connection resources", CLASSIC, {STACK_TAG("BT_AVCT")}},
    {"AVRC", "AVRCP (CT + TG)", "Control, metadata and volume notifications", CLASSIC,
        {STACK_TAG("BT_AVRC"), APP_TAG("BT_AUD_AVRC_CT"), APP_TAG("BT_AUD_AVRC_TG")}},
    {"BAL", "BTA / profile adaptation", "Bluedroid profile state machines", CLASSIC, {STACK_TAG("BT_APPL")}},
    {"BTC", "BTC / callbacks", "Bluedroid task and profile dispatch", CLASSIC, {STACK_TAG("BT_BTC")}},
    {"BTIF", "BTIF", "Bluedroid interface layer", CLASSIC, {STACK_TAG("BT_BTIF")}},
    {"OSI", "OSI / timers", "Stack alarms, allocation and OS adaptation", CLASSIC, {STACK_TAG("BT_OSI")}},
    {"AUDIO", "Audio / GMF", "Stream lifecycle, pipelines and IO", BT_DEBUG_CAP_DEBUG,
        {APP_TAG("STREAM_PROC"), APP_TAG("BT_AUD_STREAM"), APP_TAG("BT_AUD_CLASSIC_STREAM"),
         APP_TAG("ESP_GMF_IO_BT"), APP_TAG("ESP_GMF_IO_FILE")}},
    {"UI", "UI / LVGL", "UI callbacks and display integration", BT_DEBUG_CAP_DEBUG | BT_DEBUG_CAP_UI,
        {APP_TAG("BT_UI"), APP_TAG("LVGL")}},
    {"A2D_PACKETS", "A2DP packets (high load)", "Explicit opt-in; can alter audio timing", CLASSIC,
        {APP_TAG("BT_AUD_A2D_SINK")}},
    {"HFP", "HFP / RFCOMM", "Reserved for a firmware with HFP enabled", CLASSIC | BT_DEBUG_CAP_HFP,
        {STACK_TAG("BT_RFCOMM"), APP_TAG("BT_AUD_HFP_HF")}},
    {"BLE", "BLE / GATT / SMP", "Reserved for a firmware with BLE enabled", BT_DEBUG_CAP_DEBUG | BT_DEBUG_CAP_BLE,
        {STACK_TAG("BT_GATT"), STACK_TAG("BT_SMP")}},
};
_Static_assert(sizeof(modules) / sizeof(modules[0]) <= 32, "NVS module mask is 32 bits");

size_t bt_debug_module_count(void) { return sizeof(modules) / sizeof(modules[0]); }
const bt_debug_module_t *bt_debug_module_at(size_t index)
{
    return index < bt_debug_module_count() ? &modules[index] : NULL;
}
int bt_debug_module_find(const char *key)
{
    if (!key) return -1;
    for (size_t i = 0; i < bt_debug_module_count(); ++i) {
        if (strcmp(key, modules[i].key) == 0) return (int)i;
    }
    return -1;
}
bool bt_debug_policy_supported(const bt_debug_policy_t *policy, size_t index)
{
    const bt_debug_module_t *module = bt_debug_module_at(index);
    return policy && module && (policy->capabilities & module->required_caps) == module->required_caps;
}
bool bt_debug_policy_enabled(const bt_debug_policy_t *policy, size_t index)
{
    return bt_debug_policy_supported(policy, index) && (policy->enabled_mask & (1u << index)) != 0;
}

static void apply_policy(bt_debug_policy_t *policy)
{
    for (size_t i = 0; i < bt_debug_module_count(); ++i) {
        for (size_t t = 0; modules[i].tags[t].tag; ++t) {
            const bt_debug_tag_t *tag = &modules[i].tags[t];
            int level = tag->baseline;
            /* Shared tags use the most verbose request, not last-toggle-wins. */
            for (size_t j = 0; j < bt_debug_module_count(); ++j) {
                if (!bt_debug_policy_enabled(policy, j)) continue;
                for (size_t k = 0; modules[j].tags[k].tag; ++k) {
                    if (strcmp(tag->tag, modules[j].tags[k].tag) == 0) level = 4;
                }
            }
            policy->apply(tag->tag, level, policy->ctx);
        }
    }
}
bool bt_debug_policy_init(bt_debug_policy_t *policy, uint32_t caps, bt_debug_apply_fn apply, void *ctx)
{
    if (!policy || !apply) return false;
    *policy = (bt_debug_policy_t){.capabilities = caps, .apply = apply, .ctx = ctx};
    apply_policy(policy);
    return true;
}
bool bt_debug_policy_set(bt_debug_policy_t *policy, size_t index, bool enabled)
{
    if (!bt_debug_policy_supported(policy, index)) return false;
    if (enabled) policy->enabled_mask |= 1u << index;
    else policy->enabled_mask &= ~(1u << index);
    apply_policy(policy);
    return true;
}
void bt_debug_policy_restore(bt_debug_policy_t *policy, uint32_t mask)
{
    if (!policy) return;
    policy->enabled_mask = 0;
    for (size_t i = 0; i < bt_debug_module_count(); ++i) {
        if (bt_debug_policy_supported(policy, i)) policy->enabled_mask |= mask & (1u << i);
    }
    apply_policy(policy);
}

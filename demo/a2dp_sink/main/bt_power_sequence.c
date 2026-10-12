#include <stddef.h>
#include "bt_power_sequence.h"

int bt_power_sequence(bool enabled, int (*step)(bt_power_step_t, void *),
                      void *ctx, bt_power_step_t *failed)
{
    static const bt_power_step_t off[] = {BT_POWER_QUIESCE, BT_POWER_PROFILES_STOP,
        BT_POWER_HOST_STOP, BT_POWER_AUDIO_RELEASE, BT_POWER_CONTROLLER_STOP};
    static const bt_power_step_t on[] = {BT_POWER_CONTROLLER_START, BT_POWER_AUDIO_START,
        BT_POWER_ACTIVATE};
    if (failed) *failed = BT_POWER_STEP_NONE;
    if (!step) return -1;
    const bt_power_step_t *actions = enabled ? on : off;
    size_t count = enabled ? sizeof(on) / sizeof(*on) : sizeof(off) / sizeof(*off);
    for (size_t i = 0; i < count; ++i) {
        int err = step(actions[i], ctx);
        if (err) {
            if (failed) *failed = actions[i];
            return err;
        }
    }
    return 0;
}

const char *bt_power_step_name(bt_power_step_t step)
{
    static const char *names[] = {"idle", "quiesce", "profiles stop", "host stop",
        "audio release", "controller stop", "controller start", "audio start", "activate"};
    return (unsigned)step < sizeof(names) / sizeof(*names) ? names[step] : "unknown";
}

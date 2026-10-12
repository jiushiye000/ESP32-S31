#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "bt_power_sequence.h"

static unsigned calls, fail_at;
static bt_power_step_t seen[16];
static int step(bt_power_step_t action, void *ctx)
{
    assert(ctx == seen);
    seen[calls++] = action;
    return calls == fail_at ? 123 : 0;
}
int main(void)
{
    const bt_power_step_t off[] = {BT_POWER_QUIESCE, BT_POWER_PROFILES_STOP,
        BT_POWER_HOST_STOP, BT_POWER_AUDIO_RELEASE, BT_POWER_CONTROLLER_STOP};
    const bt_power_step_t on[] = {BT_POWER_CONTROLLER_START, BT_POWER_AUDIO_START,
        BT_POWER_ACTIVATE};
    for (unsigned enabled = 0; enabled < 2; ++enabled) {
        const bt_power_step_t *expected = enabled ? on : off;
        unsigned count = enabled ? 3 : 5;
        for (fail_at = 0; fail_at <= count; ++fail_at) {
            calls = 0;
            bt_power_step_t failed = BT_POWER_STEP_NONE;
            int err = bt_power_sequence(enabled, step, seen, &failed);
            assert(err == (fail_at ? 123 : 0));
            assert(calls == (fail_at ? fail_at : count));
            assert(!memcmp(seen, expected, calls * sizeof(*seen)));
            assert(failed == (fail_at ? expected[fail_at - 1] : BT_POWER_STEP_NONE));
        }
    }
    assert(bt_power_sequence(true, NULL, seen, NULL) != 0);
    assert(!strcmp(bt_power_step_name(BT_POWER_STEP_NONE), "idle"));
    for (int i = BT_POWER_QUIESCE; i <= BT_POWER_ACTIVATE; ++i)
        assert(strcmp(bt_power_step_name(i), "unknown"));
    assert(!strcmp(bt_power_step_name(99), "unknown"));
    puts("BT power ordering and every failure boundary passed");
}

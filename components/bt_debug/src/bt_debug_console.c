#include "bt_debug_console.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "esp_console.h"
#include "bt_debug.h"

static int debug_config(int argc, char **argv)
{
    if (argc == 1 || (argc == 2 && strcmp(argv[1], "status") == 0)) {
        for (size_t i = 0; i < bt_debug_module_count(); ++i) {
            const bt_debug_module_t *module = bt_debug_module_at(i);
            printf("%-12s requested=%s effective=%s available=%s\n", module->key,
                   bt_debug_enabled(i) ? "ON" : "OFF", bt_debug_overridden(i) ? "raw override" :
                   bt_debug_effective(i) ? "DEBUG" : "baseline",
                   bt_debug_supported(i) ? "yes" : "not compiled");
        }
        bt_debug_status_t state;
        bt_debug_get_status(&state);
        printf("SD requested=%s state=%s bytes=%" PRIu64 " dropped=%" PRIu32 " truncated=%" PRIu32 " errno=%d\n",
               state.sd_requested ? "ON" : "OFF", bt_debug_sd_state_name(state.sd_state), state.bytes_written,
               state.dropped, state.truncated, state.sd_errno);
        printf("File: %s\nNVS: %s%s\n", state.path, esp_err_to_name(state.settings_error), state.settings_pending ? " (saving)" : "");
        return 0;
    }
    if (argc == 3 && (!strcmp(argv[2], "on") || !strcmp(argv[2], "off"))) {
        bool enabled = strcmp(argv[2], "on") == 0;
        if (!strcmp(argv[1], "sd")) { bt_debug_sd_set(enabled); return 0; }
        if (!strcmp(argv[1], "all") && !enabled) { bt_debug_all_off(); return 0; }
        int index = bt_debug_module_find(argv[1]);
        esp_err_t err = index < 0 ? ESP_ERR_INVALID_ARG : bt_debug_set((size_t)index, enabled);
        if (err != ESP_OK) printf("Debug configuration rejected: %s\n", esp_err_to_name(err));
        return err == ESP_OK ? 0 : 1;
    }
    printf("Usage: debug [status] | debug <module-key|sd> <on|off> | debug all off\n");
    return 1;
}

esp_err_t bt_debug_register_commands(void)
{
    const esp_console_cmd_t command = {
        .command = "debug",
        .help = "Configure module debug / async SD logging; shared with the screen",
        .hint = "[status|<module-key|sd|all> <on|off>]",
        .func = debug_config,
    };
    return esp_console_cmd_register(&command);
}

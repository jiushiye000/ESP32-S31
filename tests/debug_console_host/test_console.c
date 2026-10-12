#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "esp_console.h"
#include "bt_debug.h"
#include "bt_debug_console.h"

static esp_console_cmd_t registered;
static unsigned registrations, set_calls, sd_calls, all_off_calls, status_calls;
static esp_err_t registration_result;
static bt_debug_policy_t policy;
static bool raw_override;
static bt_debug_status_t status;

const char *esp_err_to_name(esp_err_t error)
{
    switch (error) {
        case ESP_OK: return "ESP_OK";
        case ESP_ERR_INVALID_ARG: return "ESP_ERR_INVALID_ARG";
        case ESP_ERR_NOT_SUPPORTED: return "ESP_ERR_NOT_SUPPORTED";
        default: return "ESP_FAIL";
    }
}
esp_err_t esp_console_cmd_register(const esp_console_cmd_t *command)
{
    assert(command && command->command && command->help && command->hint && command->func);
    registered = *command;
    ++registrations;
    return registration_result;
}
static void apply_tag(const char *tag, int level, void *context)
{
    (void)tag; (void)level; (void)context;
}
bool bt_debug_supported(size_t index) { return bt_debug_policy_supported(&policy, index); }
bool bt_debug_enabled(size_t index) { return bt_debug_policy_enabled(&policy, index); }
bool bt_debug_effective(size_t index) { return bt_debug_enabled(index); }
bool bt_debug_overridden(size_t index) { return raw_override && bt_debug_supported(index); }
esp_err_t bt_debug_set(size_t index, bool enabled)
{
    ++set_calls;
    return bt_debug_policy_set(&policy, index, enabled) ? ESP_OK : ESP_ERR_NOT_SUPPORTED;
}
void bt_debug_all_off(void) { ++all_off_calls; bt_debug_policy_restore(&policy, 0); }
void bt_debug_sd_set(bool enabled) { ++sd_calls; status.sd_requested = enabled; }
void bt_debug_get_status(bt_debug_status_t *out) { ++status_calls; *out = status; }
const char *bt_debug_sd_state_name(bt_debug_sd_state_t state)
{
    assert(state == BT_DEBUG_SD_NO_CARD);
    return "NO CARD / serial only";
}
static int run(int argc, const char *const *arguments)
{
    char *argv[4];
    assert(argc <= 4);
    for (int i = 0; i < argc; ++i) argv[i] = (char *)arguments[i];
    return registered.func(argc, argv);
}
static void status_output(void)
{
    FILE *capture = tmpfile();
    assert(capture);
    assert(fflush(stdout) == 0);
    int saved_stdout = dup(STDOUT_FILENO);
    assert(saved_stdout >= 0 && dup2(fileno(capture), STDOUT_FILENO) >= 0);
    const char *args[] = {"debug", "status"};
    assert(run(2, args) == 0);
    assert(fflush(stdout) == 0);
    assert(dup2(saved_stdout, STDOUT_FILENO) >= 0);
    close(saved_stdout);
    rewind(capture);
    char output[8192] = {0};
    size_t size = fread(output, 1, sizeof(output) - 1, capture);
    assert(size > 0 && !ferror(capture) && feof(capture));
    assert(strstr(output, "requested=ON") && strstr(output, "effective=DEBUG"));
    assert(strstr(output, "available=not compiled"));
    assert(strstr(output, "NO CARD / serial only") && strstr(output, "dropped=7"));
    assert(strstr(output, "truncated=3") && strstr(output, "NVS: ESP_OK (saving)"));
    fclose(capture);
}

int main(void)
{
    assert(bt_debug_policy_init(&policy, BT_DEBUG_CAP_DEBUG | BT_DEBUG_CAP_CLASSIC, apply_tag, NULL));
    status = (bt_debug_status_t){.sd_state = BT_DEBUG_SD_NO_CARD, .dropped = 7,
                               .truncated = 3, .settings_pending = true};
    registration_result = ESP_FAIL;
    assert(bt_debug_register_commands() == ESP_FAIL);
    registration_result = ESP_OK;
    assert(bt_debug_register_commands() == ESP_OK && registrations == 2);
    assert(strcmp(registered.command, "debug") == 0);
    assert(strstr(registered.hint, "status"));
    const char *a2d_on[] = {"debug", "A2D", "on"};
    const char *a2d_off[] = {"debug", "A2D", "off"};
    const char *avrc_on[] = {"debug", "AVRC", "on"};
    int a2d = bt_debug_module_find("A2D"), avrc = bt_debug_module_find("AVRC");
    assert(run(3, a2d_on) == 0 && bt_debug_enabled((size_t)a2d));
    assert(!bt_debug_enabled((size_t)avrc));
    assert(run(3, avrc_on) == 0 && bt_debug_enabled((size_t)avrc));
    assert(run(3, a2d_off) == 0 && !bt_debug_enabled((size_t)a2d));
    assert(bt_debug_enabled((size_t)avrc));
    status_output();
    raw_override = true;
    const char *bare[] = {"debug"};
    assert(run(1, bare) == 0 && status_calls == 2);
    raw_override = false;
    const char *sd_on[] = {"debug", "sd", "on"};
    const char *sd_off[] = {"debug", "sd", "off"};
    const char *all_off[] = {"debug", "all", "off"};
    assert(run(3, sd_on) == 0 && status.sd_requested);
    assert(run(3, all_off) == 0 && all_off_calls == 1 && policy.enabled_mask == 0);
    assert(status.sd_requested); /* All modules OFF must not disable the SD sink. */
    assert(run(3, sd_off) == 0 && !status.sd_requested && sd_calls == 2);
    const char *hfp[] = {"debug", "HFP", "on"};
    const char *unknown[] = {"debug", "missing", "on"};
    const char *all_on[] = {"debug", "all", "on"};
    assert(run(3, hfp) == 1 && policy.enabled_mask == 0);
    unsigned before = set_calls;
    assert(run(3, unknown) == 1 && set_calls == before);
    assert(run(3, all_on) == 1 && set_calls == before);
    const char *bad_switch[] = {"debug", "A2D", "true"};
    const char *bad_arity[] = {"debug", "A2D"};
    const char *extra[] = {"debug", "A2D", "on", "extra"};
    assert(run(3, bad_switch) == 1 && run(2, bad_arity) == 1 && run(4, extra) == 1);
    assert(set_calls == before && sd_calls == 2 && all_off_calls == 1);
    puts("Debug console: registration errors, shared policy, status, SD and rejected arguments PASS");
}

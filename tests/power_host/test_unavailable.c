#define CONFIG_GMF_EXAMPLE_HFP_HF 1
#include "platform.h"
#include "../../demo/a2dp_sink/main/bt_power.c"
int xSemaphoreTake(SemaphoreHandle_t s, unsigned ticks) { (void)s;(void)ticks; return 0; }
int xSemaphoreGive(SemaphoreHandle_t s) { (void)s; return 1; }
int main(void)
{
    bt_power_status_t s;
    bt_power_get_status(&s);
    assert(s.state == BT_POWER_UNAVAILABLE);
    assert(bt_power_request(true) == ESP_ERR_NOT_SUPPORTED);
    assert(bt_power_request(false) == ESP_ERR_NOT_SUPPORTED);
    assert(bt_power_events_allowed());
    assert(bt_power_command_begin()); bt_power_command_end();
    assert(!strcmp(bt_power_state_name(BT_POWER_OFF), "OFF"));
    assert(!strcmp(bt_power_state_name(99), "UNKNOWN"));
    puts("Unsupported roles: power unavailable, existing commands unaffected PASS");
}

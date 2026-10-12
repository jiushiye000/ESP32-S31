/* Executes the real adapter with deterministic public-SDK/RTOS fakes. It does
 * not prove RF, SDK scheduling, stream stop or board playback behavior. */
#include "platform.h"
#include <string.h>
#include "../../demo/a2dp_sink/main/bt_power.c"

static void (*worker)(void *);
static esp_a2d_cb_t sdk_a2d;
static esp_avrc_ct_cb_t sdk_ct;
static esp_avrc_tg_cb_t sdk_tg;
static esp_bt_controller_status_t controller = ESP_BT_CONTROLLER_STATUS_ENABLED;
static esp_bluedroid_status_t host = ESP_BLUEDROID_STATUS_ENABLED;
static bool quiesced, sdk_live = true, drop_a2d_down, drop_disconnect;
static int fault, forwards, deinit_calls;
static unsigned stage_order[32], order_count;
static int injected(int site) { if (fault == site) { fault = 0; return ESP_FAIL; } return ESP_OK; }
static void record(unsigned site) { stage_order[order_count++] = site; assert(order_count < 32); }
SemaphoreHandle_t xSemaphoreCreateMutex(void) { SemaphoreHandle_t s = calloc(1, sizeof(*s)); s->available = 1; return s; }
SemaphoreHandle_t xSemaphoreCreateBinary(void) { return calloc(1, sizeof(struct { unsigned available; })); }
int xSemaphoreTake(SemaphoreHandle_t s, unsigned ticks) { (void)ticks; if (!s->available) return 0; --s->available; return 1; }
int xSemaphoreGive(SemaphoreHandle_t s) { s->available = 1; return 1; }
QueueHandle_t xQueueCreate(unsigned len, unsigned size) { assert(len == 1 && size == sizeof(bool)); return calloc(1, sizeof(*requests)); }
int xQueueSend(QueueHandle_t q, const void *data, unsigned ticks) { (void)ticks; if (fault == 99) { fault = 0; return 0; } if (q->count) return 0; q->enabled = *(bool *)data; q->count = 1; return 1; }
int xQueueReceive(QueueHandle_t q, void *data, unsigned ticks) { (void)ticks; if (!q->count) return 0; *(bool *)data = q->enabled; q->count = 0; return 1; }
int xTaskCreate(void (*fn)(void *), const char *n, unsigned s, void *a, unsigned p, void *h) { (void)n;(void)s;(void)a;(void)p;(void)h; worker = fn; return 1; }
EventGroupHandle_t xEventGroupCreate(void) { return calloc(1, sizeof(unsigned)); }
EventBits_t xEventGroupSetBits(EventGroupHandle_t g, EventBits_t b) { return *g |= b; }
EventBits_t xEventGroupClearBits(EventGroupHandle_t g, EventBits_t b) { unsigned old = *g; *g &= ~b; return old; }
EventBits_t xEventGroupGetBits(EventGroupHandle_t g) { return *g; }
EventBits_t xEventGroupWaitBits(EventGroupHandle_t g, EventBits_t b, int clear, int all, unsigned ticks) { (void)b;(void)clear;(void)all;(void)ticks; return *g; }
esp_bt_controller_status_t esp_bt_controller_get_status(void) { return controller; }
esp_bluedroid_status_t esp_bluedroid_get_status(void) { return host; }
esp_err_t esp_bt_controller_enable(unsigned mode) { assert(mode == 7); if (injected(7)) return ESP_FAIL; controller = ESP_BT_CONTROLLER_STATUS_ENABLED; record(7); return ESP_OK; }
esp_err_t esp_bt_controller_disable(void) { if (injected(6)) return ESP_FAIL; assert(host == ESP_BLUEDROID_STATUS_UNINITIALIZED); controller = ESP_BT_CONTROLLER_STATUS_INITED; record(6); return ESP_OK; }
esp_err_t esp_bluedroid_disable(void) { if (injected(4)) return ESP_FAIL; assert((xEventGroupGetBits(profiles) & wanted(false)) == wanted(false)); host = ESP_BLUEDROID_STATUS_INITIALIZED; record(4); return ESP_OK; }
esp_err_t esp_bluedroid_deinit(void) { if (injected(40)) return ESP_FAIL; assert(host == ESP_BLUEDROID_STATUS_INITIALIZED); host = ESP_BLUEDROID_STATUS_UNINITIALIZED; return ESP_OK; }
esp_err_t esp_bt_gap_set_scan_mode(int a, int b) { assert(a == 0 && b == 0); return injected(10); }
static void gmf_a2d(esp_a2d_cb_event_t e, esp_a2d_cb_param_t *p) { (void)e;(void)p; assert(sdk_live); ++forwards; }
static void gmf_ct(esp_avrc_ct_cb_event_t e, esp_avrc_ct_cb_param_t *p) { (void)e;(void)p; assert(sdk_live); ++forwards; }
static void gmf_tg(esp_avrc_tg_cb_event_t e, esp_avrc_tg_cb_param_t *p) { (void)e;(void)p; assert(sdk_live); ++forwards; }
esp_err_t __real_esp_a2d_register_callback(esp_a2d_cb_t cb) { sdk_a2d = cb; return injected(11); }
esp_err_t __real_esp_avrc_ct_register_callback(esp_avrc_ct_cb_t cb) { sdk_ct = cb; return injected(12); }
esp_err_t __real_esp_avrc_tg_register_callback(esp_avrc_tg_cb_t cb) { sdk_tg = cb; return injected(13); }
esp_err_t __real_esp_a2d_sink_init(void) { esp_a2d_cb_param_t p = {.a2d_prof_stat.init_state = ESP_A2D_INIT_SUCCESS}; sdk_a2d(ESP_A2D_PROF_STATE_EVT, &p); return ESP_OK; }
esp_err_t __real_esp_avrc_ct_init(void) { esp_avrc_ct_cb_param_t p = {.avrc_ct_init_stat.state = ESP_AVRC_INIT_SUCCESS}; sdk_ct(ESP_AVRC_CT_PROF_STATE_EVT, &p); return ESP_OK; }
esp_err_t __real_esp_avrc_tg_init(void) { esp_avrc_tg_cb_param_t p = {.avrc_tg_init_stat.state = ESP_AVRC_INIT_SUCCESS}; sdk_tg(ESP_AVRC_TG_PROF_STATE_EVT, &p); return ESP_OK; }
esp_err_t esp_avrc_ct_deinit(void) { if (injected(2)) return ESP_FAIL; record(2); esp_avrc_ct_cb_param_t p = {.avrc_ct_init_stat.state = ESP_AVRC_DEINIT_SUCCESS}; sdk_ct(ESP_AVRC_CT_PROF_STATE_EVT, &p); return ESP_OK; }
esp_err_t esp_avrc_tg_deinit(void) { if (injected(20)) return ESP_FAIL; record(20); esp_avrc_tg_cb_param_t p = {.avrc_tg_init_stat.state = ESP_AVRC_DEINIT_SUCCESS}; sdk_tg(ESP_AVRC_TG_PROF_STATE_EVT, &p); return ESP_OK; }
esp_err_t esp_a2d_sink_deinit(void) { if (injected(3)) return ESP_FAIL; assert(*profiles & CT_DOWN); assert(*profiles & TG_DOWN); record(3); ++deinit_calls; if (!drop_a2d_down) { esp_a2d_cb_param_t p = {.a2d_prof_stat.init_state = ESP_A2D_DEINIT_SUCCESS}; sdk_a2d(ESP_A2D_PROF_STATE_EVT, &p); } return ESP_OK; }
esp_err_t esp_a2d_sink_disconnect(uint8_t *bda) { assert(bda[0] == 0x80); if (injected(30)) return ESP_FAIL; if (!drop_disconnect) { esp_a2d_cb_param_t p = {.conn_stat.state = ESP_A2D_CONNECTION_STATE_DISCONNECTED}; sdk_a2d(ESP_A2D_CONNECTION_STATE_EVT, &p); } return ESP_OK; }
esp_err_t esp_bt_audio_init(esp_bt_audio_config_t *cfg) { assert(cfg == audio_config); if (injected(8)) return ESP_FAIL; sdk_live = true; host = ESP_BLUEDROID_STATUS_ENABLED; record(8); assert(__wrap_esp_avrc_ct_init() == ESP_OK); assert(__wrap_esp_avrc_ct_register_callback(gmf_ct) == ESP_OK); assert(__wrap_esp_avrc_tg_init() == ESP_OK); assert(__wrap_esp_avrc_tg_register_callback(gmf_tg) == ESP_OK); assert(__wrap_esp_a2d_sink_init() == ESP_OK); assert(__wrap_esp_a2d_register_callback(gmf_a2d) == ESP_OK); return ESP_OK; }
void esp_bt_audio_deinit(void) { assert(quiesced && host == ESP_BLUEDROID_STATUS_UNINITIALIZED); assert((*profiles & wanted(false)) == wanted(false)); record(5); sdk_live = false; }
esp_err_t stream_proc_quiesce(void) { if (injected(1)) return ESP_FAIL; record(1); quiesced = true; return ESP_OK; }
void stream_proc_resume(void) { quiesced = false; }
esp_err_t esp_timer_create(const esp_timer_create_args_t *args, esp_timer_handle_t *out) { *out = malloc(sizeof(**out)); **out = *args; return ESP_OK; }
esp_err_t esp_timer_stop_blocking(esp_timer_handle_t timer, unsigned ticks) { (void)timer;(void)ticks; return injected(14); }
esp_err_t esp_timer_start_once(esp_timer_handle_t timer, uint64_t us) { assert(us == 300000); if (injected(15)) return ESP_FAIL; if (fault != 16) timer->callback(timer->arg); else fault = 0; return ESP_OK; }
const char *esp_err_to_name(esp_err_t e) { (void)e; return "fake"; }
static esp_err_t activate(void) { if (injected(9)) return ESP_FAIL; record(9); return ESP_OK; }
static bt_power_state_t state_now(void) { bt_power_status_t s; bt_power_get_status(&s); return s.state; }
static void request_run(bool enabled) { assert(bt_power_request(enabled) == ESP_OK); assert(!bt_power_events_allowed()); assert(!bt_power_command_begin()); worker(NULL); order_count = 0; }
static void recover(void) { request_run(false); assert(state_now() == BT_POWER_OFF); request_run(true); assert(state_now() == BT_POWER_ON); }
int main(void)
{
    bt_power_get_status(NULL);
    assert(bt_power_request(false) == ESP_ERR_NOT_SUPPORTED);
    assert(bt_power_command_begin()); bt_power_command_end();
    assert(bt_power_prepare(NULL, 7, activate) == ESP_ERR_INVALID_ARG);
    assert(bt_power_ready() == ESP_ERR_INVALID_STATE);
    static esp_bt_audio_config_t cfg = {.host_config = &cfg, .classic.roles = 7};
    assert(bt_power_prepare(&cfg, 7, activate) == ESP_OK);
    assert(bt_power_prepare(&cfg, 7, activate) == ESP_ERR_INVALID_ARG);
    assert(esp_bt_audio_init(&cfg) == ESP_OK);
    assert(bt_power_ready() == ESP_OK); order_count = 0;
    assert(bt_power_command_begin()); assert(!bt_power_command_begin()); bt_power_command_end();
    assert(bt_power_request(true) == ESP_OK && requests->count == 0);
    fault = 99; assert(bt_power_request(false) == ESP_ERR_NO_MEM && state_now() == BT_POWER_ON);
    assert(bt_power_request(false) == ESP_OK);
    assert(bt_power_request(true) == ESP_ERR_INVALID_STATE);
    worker(NULL);
    const unsigned expected[] = {1, 2, 20, 3, 4, 5, 6};
    assert(order_count == 7 && !memcmp(stage_order, expected, sizeof(expected)));
    order_count = 0; assert(state_now() == BT_POWER_OFF && !sdk_live);
    assert(bt_power_request(false) == ESP_OK && requests->count == 0);
    assert(!bt_power_command_begin());
    request_run(true); assert(state_now() == BT_POWER_ON && sdk_live && !quiesced);
    for (unsigned i = 0; i < 10; ++i) recover();
    const int errors[] = {1, 10, 14, 15, 16, 2, 20, 3, 4, 40, 6};
    for (unsigned i = 0; i < sizeof(errors)/sizeof(*errors); ++i) {
        fault = errors[i]; request_run(false); assert(state_now() == BT_POWER_ERROR);
        assert(bt_power_request(true) == ESP_ERR_INVALID_STATE); recover();
    }
    esp_a2d_cb_param_t link = {.conn_stat = {.state = ESP_A2D_CONNECTION_STATE_CONNECTED, .remote_bda = {0x80}}};
    sdk_a2d(ESP_A2D_CONNECTION_STATE_EVT, &link);
    drop_disconnect = true; request_run(false); assert(state_now() == BT_POWER_ERROR && sdk_live);
    drop_disconnect = false; recover();
    sdk_a2d(ESP_A2D_CONNECTION_STATE_EVT, &link);
    fault = 30; request_run(false); assert(state_now() == BT_POWER_ERROR); recover();
    drop_a2d_down = true; int before = deinit_calls;
    request_run(false); assert(state_now() == BT_POWER_ERROR && sdk_live);
    request_run(false); assert(deinit_calls == before + 1 && state_now() == BT_POWER_ERROR);
    esp_a2d_cb_param_t down = {.a2d_prof_stat.init_state = ESP_A2D_DEINIT_SUCCESS};
    sdk_a2d(ESP_A2D_PROF_STATE_EVT, &down); drop_a2d_down = false; recover();
    esp_avrc_ct_cb_param_t bad_ct = {.avrc_ct_init_stat.state = ESP_AVRC_DEINIT_FAIL};
    esp_avrc_tg_cb_param_t bad_tg = {.avrc_tg_init_stat.state = ESP_AVRC_DEINIT_FAIL};
    xEventGroupClearBits(profiles, CT_DOWN | TG_DOWN);
    sdk_ct(ESP_AVRC_CT_PROF_STATE_EVT, &bad_ct); sdk_tg(ESP_AVRC_TG_PROF_STATE_EVT, &bad_tg);
    assert(!(*profiles & (CT_DOWN | TG_DOWN)));
    for (int site = 7; site <= 9; ++site) {
        request_run(false); fault = site; request_run(true); assert(state_now() == BT_POWER_ERROR); recover();
    }
    assert(forwards > 50);
    assert(run_step(BT_POWER_STEP_NONE, NULL) == ESP_ERR_INVALID_ARG);
    puts("Real Bluetooth power adapter: 10 cycles, all stage faults, async timeout/retry, early init, callback forwarding and command gates PASS");
}

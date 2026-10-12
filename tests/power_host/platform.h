#pragma once
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#define CONFIG_BT_BLUEDROID_ENABLED 1
#define CONFIG_GMF_EXAMPLE_A2DP_SINK 1
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_NOT_SUPPORTED 0x106
#define ESP_ERR_TIMEOUT 0x107
typedef int esp_err_t;
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define portMAX_DELAY 0xffffffff
#define pdMS_TO_TICKS(x) (x)
#define BIT0 1
#define BIT1 2
#define BIT2 4
#define BIT3 8
#define BIT4 16
#define BIT5 32
#define BIT6 64
typedef unsigned portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(m) ((void)(m))
#define portEXIT_CRITICAL(m) ((void)(m))
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
typedef struct { unsigned available; } *SemaphoreHandle_t;
SemaphoreHandle_t xSemaphoreCreateMutex(void);
SemaphoreHandle_t xSemaphoreCreateBinary(void);
int xSemaphoreTake(SemaphoreHandle_t, unsigned);
int xSemaphoreGive(SemaphoreHandle_t);
typedef struct { unsigned count; bool enabled; } *QueueHandle_t;
QueueHandle_t xQueueCreate(unsigned, unsigned);
int xQueueSend(QueueHandle_t, const void *, unsigned);
int xQueueReceive(QueueHandle_t, void *, unsigned);
int xTaskCreate(void (*)(void *), const char *, unsigned, void *, unsigned, void *);
typedef unsigned EventBits_t;
typedef unsigned *EventGroupHandle_t;
EventGroupHandle_t xEventGroupCreate(void);
EventBits_t xEventGroupSetBits(EventGroupHandle_t, EventBits_t);
EventBits_t xEventGroupClearBits(EventGroupHandle_t, EventBits_t);
EventBits_t xEventGroupGetBits(EventGroupHandle_t);
EventBits_t xEventGroupWaitBits(EventGroupHandle_t, EventBits_t, int, int, unsigned);
typedef enum { ESP_BT_CONTROLLER_STATUS_IDLE, ESP_BT_CONTROLLER_STATUS_INITED, ESP_BT_CONTROLLER_STATUS_ENABLED } esp_bt_controller_status_t;
typedef enum { ESP_BLUEDROID_STATUS_UNINITIALIZED, ESP_BLUEDROID_STATUS_INITIALIZED, ESP_BLUEDROID_STATUS_ENABLED } esp_bluedroid_status_t;
esp_bt_controller_status_t esp_bt_controller_get_status(void);
esp_bluedroid_status_t esp_bluedroid_get_status(void);
esp_err_t esp_bt_controller_enable(unsigned);
esp_err_t esp_bt_controller_disable(void);
esp_err_t esp_bluedroid_disable(void);
esp_err_t esp_bluedroid_deinit(void);
#define ESP_BT_NON_CONNECTABLE 0
#define ESP_BT_NON_DISCOVERABLE 0
esp_err_t esp_bt_gap_set_scan_mode(int, int);
typedef enum { ESP_A2D_PROF_STATE_EVT, ESP_A2D_CONNECTION_STATE_EVT, ESP_A2D_AUDIO_STATE_EVT } esp_a2d_cb_event_t;
typedef enum { ESP_A2D_CONNECTION_STATE_DISCONNECTED, ESP_A2D_CONNECTION_STATE_CONNECTING, ESP_A2D_CONNECTION_STATE_CONNECTED, ESP_A2D_CONNECTION_STATE_DISCONNECTING } esp_a2d_connection_state_t;
enum { ESP_A2D_DEINIT_SUCCESS, ESP_A2D_INIT_SUCCESS };
typedef struct { struct { int init_state; } a2d_prof_stat; struct { esp_a2d_connection_state_t state; uint8_t remote_bda[6]; } conn_stat; } esp_a2d_cb_param_t;
typedef void (*esp_a2d_cb_t)(esp_a2d_cb_event_t, esp_a2d_cb_param_t *);
typedef enum { ESP_AVRC_CT_PROF_STATE_EVT, ESP_AVRC_CT_CONNECTION_STATE_EVT } esp_avrc_ct_cb_event_t;
typedef enum { ESP_AVRC_TG_PROF_STATE_EVT, ESP_AVRC_TG_CONNECTION_STATE_EVT } esp_avrc_tg_cb_event_t;
enum { ESP_AVRC_INIT_SUCCESS, ESP_AVRC_INIT_ALREADY, ESP_AVRC_INIT_FAIL, ESP_AVRC_DEINIT_SUCCESS, ESP_AVRC_DEINIT_ALREADY, ESP_AVRC_DEINIT_FAIL };
typedef struct { struct { int state; } avrc_ct_init_stat; } esp_avrc_ct_cb_param_t;
typedef struct { struct { int state; } avrc_tg_init_stat; } esp_avrc_tg_cb_param_t;
typedef void (*esp_avrc_ct_cb_t)(esp_avrc_ct_cb_event_t, esp_avrc_ct_cb_param_t *);
typedef void (*esp_avrc_tg_cb_t)(esp_avrc_tg_cb_event_t, esp_avrc_tg_cb_param_t *);
esp_err_t esp_avrc_ct_deinit(void);
esp_err_t esp_avrc_tg_deinit(void);
esp_err_t esp_a2d_sink_deinit(void);
esp_err_t esp_a2d_sink_disconnect(uint8_t *);
#define ESP_BT_AUDIO_CLASSIC_ROLE_A2DP_SNK 1
#define ESP_BT_AUDIO_CLASSIC_ROLE_AVRC_CT 2
#define ESP_BT_AUDIO_CLASSIC_ROLE_AVRC_TG 4
typedef struct { void *host_config; struct { unsigned roles; } classic; } esp_bt_audio_config_t;
esp_err_t esp_bt_audio_init(esp_bt_audio_config_t *);
void esp_bt_audio_deinit(void);
typedef struct { void (*callback)(void *); void *arg; const char *name; } esp_timer_create_args_t;
typedef esp_timer_create_args_t *esp_timer_handle_t;
esp_err_t esp_timer_create(const esp_timer_create_args_t *, esp_timer_handle_t *);
esp_err_t esp_timer_start_once(esp_timer_handle_t, uint64_t);
esp_err_t esp_timer_stop_blocking(esp_timer_handle_t, unsigned);
const char *esp_err_to_name(esp_err_t);
typedef void *esp_gmf_pool_handle_t;
typedef void *esp_gmf_pipeline_handle_t;
typedef void *esp_bt_audio_stream_handle_t;
typedef unsigned esp_bt_audio_stream_state_t;

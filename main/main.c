#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ina_driver.h"

static const char* TAG = "main";

void app_main(void)
{
    esp_err_t init_status = ina169_init(GPIO_NUM_1);
    if (init_status != ESP_OK)
    {
        ESP_LOGE(TAG, "INA169 ADC initialization failed: %s", esp_err_to_name(init_status));
    }
    else
    {
        vTaskDelay(pdMS_TO_TICKS(10));
        esp_err_t zero_status = ina169_calibrate_zero();
        if (zero_status != ESP_OK)
        {
            ESP_LOGW(TAG, "INA169 zero calibration failed: %s", esp_err_to_name(zero_status));
        }
    }

    while (1)
    {
        if (init_status == ESP_OK)
        {
            ina169_measurement_t measurement;
            esp_err_t read_status = ina169_read(&measurement);
            if (read_status == ESP_OK)
            {
                if (measurement.voltage_calibrated)
                {
                    if (measurement.zero_calibrated)
                    {
                        ESP_LOGI(TAG, "INA169: %d mV (zero %d mV), estimated current %" PRIu32 " mA (ADC avg raw=%d)",
                                 measurement.voltage_mv, measurement.zero_offset_mv,
                                 measurement.estimated_current_ma, measurement.raw);
                    }
                    else
                    {
                        ESP_LOGI(TAG, "INA169: %d mV (zero calibration unavailable, ADC avg raw=%d)",
                                 measurement.voltage_mv, measurement.raw);
                    }
                }
                else
                {
                    ESP_LOGI(TAG, "INA169 output ADC raw=%d (voltage calibration unavailable)",
                             measurement.raw);
                }
            }
            else
            {
                ESP_LOGE(TAG, "INA169 ADC read failed: %s", esp_err_to_name(read_status));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
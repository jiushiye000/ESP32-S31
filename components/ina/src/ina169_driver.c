#include "ina_driver.h"

#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

static const char* TAG = "ina169";
static adc_oneshot_unit_handle_t s_adc_handle;
static adc_cali_handle_t s_adc_cali_handle;
static adc_unit_t s_adc_unit;
static adc_channel_t s_adc_channel;
static int s_zero_offset_mv;
static bool s_zero_calibrated;

#define INA169_ADC_AVERAGE_SAMPLES 16
#define INA169_SHUNT_RESISTOR_MILLIOHMS 100
#define INA169_LOAD_RESISTOR_OHMS 10000
#define INA169_TRANSCONDUCTANCE_OHMS 5000

static void init_calibration(adc_unit_t unit, adc_channel_t channel)
{
    adc_cali_scheme_ver_t scheme_mask = 0;
    if (adc_cali_check_scheme(&scheme_mask) != ESP_OK)
    {
        ESP_LOGW(TAG, "ADC calibration unavailable; raw readings only");
        return;
    }

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (scheme_mask & ADC_CALI_SCHEME_VER_CURVE_FITTING)
    {
        const adc_cali_curve_fitting_config_t config = {
            .unit_id = unit,
            .chan = channel,
            .atten = ADC_ATTEN_DB_0,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        if (adc_cali_create_scheme_curve_fitting(&config, &s_adc_cali_handle) == ESP_OK)
        {
            return;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (scheme_mask & ADC_CALI_SCHEME_VER_LINE_FITTING)
    {
        const adc_cali_line_fitting_config_t config = {
            .unit_id = unit,
            .atten = ADC_ATTEN_DB_0,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        if (adc_cali_create_scheme_line_fitting(&config, &s_adc_cali_handle) == ESP_OK)
        {
            return;
        }
    }
#endif

    ESP_LOGW(TAG, "ADC calibration unavailable; raw readings only");
}

esp_err_t ina169_init(gpio_num_t signal_gpio)
{
    if (s_adc_handle != NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = adc_oneshot_io_to_channel(signal_gpio, &s_adc_unit, &s_adc_channel);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "GPIO%d is not an ADC input on this target", signal_gpio);
        return err;
    }

    const adc_oneshot_unit_init_cfg_t unit_config = {
        .unit_id = s_adc_unit,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    err = adc_oneshot_new_unit(&unit_config, &s_adc_handle);
    if (err != ESP_OK)
    {
        return err;
    }

    const adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN_DB_0,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    err = adc_oneshot_config_channel(s_adc_handle, s_adc_channel, &channel_config);
    if (err != ESP_OK)
    {
        adc_oneshot_del_unit(s_adc_handle);
        s_adc_handle = NULL;
        return err;
    }

    init_calibration(s_adc_unit, s_adc_channel);
    ESP_LOGI(TAG, "INA169 analog output configured on GPIO%d", signal_gpio);
    return ESP_OK;
}

esp_err_t ina169_read(ina169_measurement_t* measurement)
{
    if (measurement == NULL || s_adc_handle == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    int raw_sum = 0;
    for (int sample = 0; sample < INA169_ADC_AVERAGE_SAMPLES; ++sample)
    {
        int raw_sample;
        esp_err_t err = adc_oneshot_read(s_adc_handle, s_adc_channel, &raw_sample);
        if (err != ESP_OK)
        {
            return err;
        }
        raw_sum += raw_sample;
    }
    measurement->raw = (raw_sum + INA169_ADC_AVERAGE_SAMPLES / 2) /
                       INA169_ADC_AVERAGE_SAMPLES;

    measurement->voltage_mv = 0;
    measurement->zero_offset_mv = s_zero_offset_mv;
    measurement->estimated_current_ma = 0;
    measurement->voltage_calibrated = false;
    measurement->zero_calibrated = s_zero_calibrated;
    if (s_adc_cali_handle != NULL &&
        adc_cali_raw_to_voltage(s_adc_cali_handle, measurement->raw,
                                &measurement->voltage_mv) == ESP_OK)
    {
        measurement->voltage_calibrated = true;
        if (s_zero_calibrated)
        {
            int corrected_voltage_mv = measurement->voltage_mv - s_zero_offset_mv;
            if (corrected_voltage_mv > 0)
            {
                measurement->estimated_current_ma =
                    (uint32_t)((uint64_t)corrected_voltage_mv * INA169_TRANSCONDUCTANCE_OHMS * 1000 /
                               (INA169_SHUNT_RESISTOR_MILLIOHMS * INA169_LOAD_RESISTOR_OHMS));
            }
        }
    }
    return ESP_OK;
}

esp_err_t ina169_calibrate_zero(void)
{
    if (s_adc_handle == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_adc_cali_handle == NULL)
    {
        return ESP_ERR_NOT_SUPPORTED;
    }

    ina169_measurement_t measurement;
    esp_err_t err = ina169_read(&measurement);
    if (err != ESP_OK)
    {
        return err;
    }
    if (!measurement.voltage_calibrated)
    {
        return ESP_ERR_NOT_SUPPORTED;
    }

    s_zero_offset_mv = measurement.voltage_mv;
    s_zero_calibrated = true;
    ESP_LOGI(TAG, "Zero-current offset calibrated: %d mV", s_zero_offset_mv);
    return ESP_OK;
}
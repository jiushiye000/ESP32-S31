#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"

/** INA169 ADC measurement. Current estimation requires both calibrations. */
typedef struct
{
    int raw;                       /**< Averaged ADC raw code. */
    int voltage_mv;                /**< Calibrated INA169 output voltage in mV. */
    int zero_offset_mv;            /**< Zero-current output offset in mV. */
    uint32_t estimated_current_ma; /**< Zero-corrected estimated current in mA. */
    bool voltage_calibrated;       /**< True when raw-to-voltage calibration succeeded. */
    bool zero_calibrated;          /**< True after ina169_calibrate_zero() succeeded. */
} ina169_measurement_t;

/** Converted measurements shared by the INA219 and INA226 I2C drivers. */
typedef struct
{
    uint16_t bus_voltage_raw; /**< Raw bus-voltage register; encoding depends on model. */
    uint32_t bus_voltage_uv;  /**< Bus voltage in uV. */
    int32_t shunt_voltage_nv; /**< Shunt voltage in nV. */
    int32_t current_ua;       /**< Current in uA. */
    uint32_t power_uw;        /**< Power in uW. */
} ina_i2c_measurement_t;

/** Initialize INA169 ADC input on signal_gpio. */
esp_err_t ina169_init(gpio_num_t signal_gpio);
/** Capture the INA169 output voltage at zero load current as its offset. */
esp_err_t ina169_calibrate_zero(void);
/** Read the INA169 output and populate measurement. */
esp_err_t ina169_read(ina169_measurement_t* measurement);

/** Initialize the INA219 I2C sensor using the driver's configured bus. */
esp_err_t ina219_init(void);
/** Read INA219 bus, shunt, current, and power measurements. */
esp_err_t ina219_read(ina_i2c_measurement_t* measurement);

/** Initialize the INA226 I2C sensor using the driver's configured bus. */
esp_err_t ina226_init(void);
/** Read INA226 bus, shunt, current, and power measurements. */
esp_err_t ina226_read(ina_i2c_measurement_t* measurement);
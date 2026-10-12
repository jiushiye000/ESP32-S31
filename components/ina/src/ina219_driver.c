#include "ina_driver.h"

#include <stddef.h>

#include "driver/i2c_master.h"
#include "esp_log.h"

#define INA219_SDA_GPIO GPIO_NUM_1
#define INA219_SCL_GPIO GPIO_NUM_0
#define INA219_I2C_FREQUENCY_HZ 100000
#define INA219_I2C_ADDRESS 0x40
#define INA219_SHUNT_RESISTOR_MILLIOHMS 100
#define INA219_MAX_CURRENT_UA 800000

#define INA219_REG_CONFIG 0x00
#define INA219_REG_SHUNT_VOLTAGE 0x01
#define INA219_REG_BUS_VOLTAGE 0x02
#define INA219_REG_POWER 0x03
#define INA219_REG_CURRENT 0x04
#define INA219_REG_CALIBRATION 0x05

#define INA219_CONFIG_CONTINUOUS 0x3D57
#define INA219_CALIBRATION_FACTOR 40960000U
#define INA219_BUS_VOLTAGE_LSB_UV 4000U
#define INA219_SHUNT_VOLTAGE_LSB_NV 10000U
#define INA219_POWER_LSB_MULTIPLIER 20U

static const char* TAG = "ina219";
static i2c_master_bus_handle_t s_i2c_bus;
static i2c_master_dev_handle_t s_device;
static uint32_t s_current_lsb_ua;

static esp_err_t write_register(uint8_t reg, uint16_t value)
{
    uint8_t data[] = {reg, (uint8_t)(value >> 8), (uint8_t)value};
    return i2c_master_transmit(s_device, data, sizeof(data), 1000);
}

static esp_err_t read_register(uint8_t reg, uint16_t* value)
{
    uint8_t data[2];
    esp_err_t err = i2c_master_transmit_receive(s_device, &reg, sizeof(reg),
                                                data, sizeof(data), 1000);
    if (err != ESP_OK)
    {
        return err;
    }

    *value = ((uint16_t)data[0] << 8) | data[1];
    return ESP_OK;
}

static int32_t decode_signed_value(uint16_t value)
{
    return (value & 0x8000U) ? (int32_t)value - 65536 : value;
}

static void cleanup_i2c(void)
{
    if (s_device != NULL)
    {
        i2c_master_bus_rm_device(s_device);
        s_device = NULL;
    }
    if (s_i2c_bus != NULL)
    {
        i2c_del_master_bus(s_i2c_bus);
        s_i2c_bus = NULL;
    }
}

esp_err_t ina219_init(void)
{
    if (s_device != NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = INA219_SDA_GPIO,
        .scl_io_num = INA219_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&bus_config, &s_i2c_bus);
    if (err != ESP_OK)
    {
        return err;
    }

    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = INA219_I2C_ADDRESS,
        .scl_speed_hz = INA219_I2C_FREQUENCY_HZ,
    };
    err = i2c_master_bus_add_device(s_i2c_bus, &device_config, &s_device);
    if (err != ESP_OK)
    {
        cleanup_i2c();
        return err;
    }

    s_current_lsb_ua = (INA219_MAX_CURRENT_UA + 32767U) / 32768U;
    const uint32_t calibration = INA219_CALIBRATION_FACTOR /
                                 (s_current_lsb_ua * INA219_SHUNT_RESISTOR_MILLIOHMS);

    err = write_register(INA219_REG_CONFIG, INA219_CONFIG_CONTINUOUS);
    if (err == ESP_OK)
    {
        err = write_register(INA219_REG_CALIBRATION, (uint16_t)calibration);
    }
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure INA219: %s", esp_err_to_name(err));
        cleanup_i2c();
    }
    return err;
}

esp_err_t ina219_read(ina_i2c_measurement_t* measurement)
{
    if (measurement == NULL || s_device == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    uint16_t raw_bus_voltage;
    uint16_t raw_shunt_voltage;
    uint16_t raw_current;
    uint16_t raw_power;

    esp_err_t err = read_register(INA219_REG_BUS_VOLTAGE, &raw_bus_voltage);
    if (err == ESP_OK)
    {
        err = read_register(INA219_REG_SHUNT_VOLTAGE, &raw_shunt_voltage);
    }
    if (err == ESP_OK)
    {
        err = read_register(INA219_REG_CURRENT, &raw_current);
    }
    if (err == ESP_OK)
    {
        err = read_register(INA219_REG_POWER, &raw_power);
    }
    if (err != ESP_OK)
    {
        return err;
    }

    measurement->bus_voltage_raw = raw_bus_voltage;
    measurement->bus_voltage_uv = (uint32_t)(raw_bus_voltage >> 3) * INA219_BUS_VOLTAGE_LSB_UV;
    measurement->shunt_voltage_nv = decode_signed_value(raw_shunt_voltage) *
                                    (int32_t)INA219_SHUNT_VOLTAGE_LSB_NV;
    measurement->current_ua = decode_signed_value(raw_current) * (int32_t)s_current_lsb_ua;
    measurement->power_uw = (uint32_t)raw_power * INA219_POWER_LSB_MULTIPLIER * s_current_lsb_ua;
    return ESP_OK;
}
#include "ina_driver.h"

#include <stddef.h>

#include "driver/i2c_master.h"
#include "esp_log.h"

#define INA226_SDA_GPIO GPIO_NUM_1
#define INA226_SCL_GPIO GPIO_NUM_0
#define INA226_I2C_FREQUENCY_HZ 100000
#define INA226_I2C_ADDRESS 0x44
#define INA226_SHUNT_RESISTOR_MILLIOHMS 100
#define INA226_MAX_CURRENT_UA 800000

#define INA226_REG_CONFIG 0x00
#define INA226_REG_SHUNT_VOLTAGE 0x01
#define INA226_REG_BUS_VOLTAGE 0x02
#define INA226_REG_POWER 0x03
#define INA226_REG_CURRENT 0x04
#define INA226_REG_CALIBRATION 0x05
#define INA226_REG_MANUFACTURER_ID 0xFE

#define INA226_CONFIG_CONTINUOUS 0x4127
#define INA226_MANUFACTURER_ID 0x5449
#define INA226_CALIBRATION_FACTOR 5120000U
#define INA226_BUS_VOLTAGE_LSB_UV 1250U
#define INA226_SHUNT_VOLTAGE_LSB_NV 2500U
#define INA226_POWER_LSB_MULTIPLIER 25U

static const char* TAG = "ina226";
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

static void scan_i2c_addresses(void)
{
    uint8_t ack_count = 0;
    for (uint16_t address = 0x08; address <= 0x77; ++address)
    {
        if (i2c_master_probe(s_i2c_bus, address, 50) == ESP_OK)
        {
            ESP_LOGW(TAG, "I2C ACK at address 0x%02" PRIX16, address);
            ++ack_count;
        }
    }
    if (ack_count == 0)
    {
        ESP_LOGE(TAG, "I2C scan found no responding devices on SDA GPIO%d / SCL GPIO%d",
                 INA226_SDA_GPIO, INA226_SCL_GPIO);
    }
}

esp_err_t ina226_init(void)
{
    if (s_device != NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = INA226_SDA_GPIO,
        .scl_io_num = INA226_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_LOGI(TAG, "I2C pins: SDA=GPIO%d, SCL=GPIO%d",
             INA226_SDA_GPIO, INA226_SCL_GPIO);
    esp_err_t err = i2c_new_master_bus(&bus_config, &s_i2c_bus);
    if (err != ESP_OK)
    {
        return err;
    }

    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = INA226_I2C_ADDRESS,
        .scl_speed_hz = INA226_I2C_FREQUENCY_HZ,
    };
    err = i2c_master_bus_add_device(s_i2c_bus, &device_config, &s_device);
    if (err != ESP_OK)
    {
        cleanup_i2c();
        return err;
    }

    uint16_t manufacturer_id = 0;
    err = read_register(INA226_REG_MANUFACTURER_ID, &manufacturer_id);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read manufacturer ID at I2C address 0x%02X: %s",
                 INA226_I2C_ADDRESS, esp_err_to_name(err));
        scan_i2c_addresses();
    }
    else if (manufacturer_id != INA226_MANUFACTURER_ID)
    {
        ESP_LOGE(TAG, "Unexpected manufacturer ID 0x%04X at address 0x%02X (expected 0x%04X)",
                 manufacturer_id, INA226_I2C_ADDRESS, INA226_MANUFACTURER_ID);
    }
    if (err != ESP_OK || manufacturer_id != INA226_MANUFACTURER_ID)
    {
        cleanup_i2c();
        return err == ESP_OK ? ESP_ERR_INVALID_RESPONSE : err;
    }

    s_current_lsb_ua = (INA226_MAX_CURRENT_UA + 32767U) / 32768U;
    const uint32_t calibration = INA226_CALIBRATION_FACTOR /
                                 (s_current_lsb_ua * INA226_SHUNT_RESISTOR_MILLIOHMS);

    err = write_register(INA226_REG_CONFIG, INA226_CONFIG_CONTINUOUS);
    if (err == ESP_OK)
    {
        err = write_register(INA226_REG_CALIBRATION, (uint16_t)calibration);
    }
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure INA226: %s", esp_err_to_name(err));
        cleanup_i2c();
    }
    return err;
}

esp_err_t ina226_read(ina_i2c_measurement_t* measurement)
{
    if (measurement == NULL || s_device == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    uint16_t raw_bus_voltage;
    uint16_t raw_shunt_voltage;
    uint16_t raw_current;
    uint16_t raw_power;

    esp_err_t err = read_register(INA226_REG_BUS_VOLTAGE, &raw_bus_voltage);
    if (err == ESP_OK)
    {
        err = read_register(INA226_REG_SHUNT_VOLTAGE, &raw_shunt_voltage);
    }
    if (err == ESP_OK)
    {
        err = read_register(INA226_REG_CURRENT, &raw_current);
    }
    if (err == ESP_OK)
    {
        err = read_register(INA226_REG_POWER, &raw_power);
    }
    if (err != ESP_OK)
    {
        return err;
    }

    measurement->bus_voltage_raw = raw_bus_voltage;
    measurement->bus_voltage_uv = (uint32_t)raw_bus_voltage * INA226_BUS_VOLTAGE_LSB_UV;
    measurement->shunt_voltage_nv = decode_signed_value(raw_shunt_voltage) *
                                    (int32_t)INA226_SHUNT_VOLTAGE_LSB_NV;
    measurement->current_ua = decode_signed_value(raw_current) * (int32_t)s_current_lsb_ua;
    measurement->power_uw = (uint32_t)raw_power * INA226_POWER_LSB_MULTIPLIER * s_current_lsb_ua;
    return ESP_OK;
}
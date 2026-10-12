set(REQ esp_adc esp_driver_gpio esp_driver_i2c)

idf_component_register(SRCS
    "src/ina169_driver.c"
    "src/ina219_driver.c"
    "src/ina226_driver.c"
    INCLUDE_DIRS "."
    REQUIRES ${REQ}
    EMBED_FILES
)

file(GLOB SRCS "main.c" "*.c")
set(REQ esp_driver_gpio
    esp_driver_i2c
    ina)

idf_component_register(SRCS ${SRCS}
    INCLUDE_DIRS "."
    REQUIRES ${REQ}
    EMBED_FILES
)

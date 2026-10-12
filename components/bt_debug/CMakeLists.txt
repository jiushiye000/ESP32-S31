idf_component_register(
    SRCS "src/bt_debug.c" "src/bt_debug_policy.c" "src/bt_debug_storage.c"
         "src/bt_debug_console.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_common
    PRIV_REQUIRES esp_board_manager nvs_flash log freertos esp_hw_support console
)

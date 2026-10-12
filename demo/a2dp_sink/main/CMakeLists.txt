set(ui_srcs "")
if(CONFIG_EXAMPLE_BT_UI_ENABLE)
    list(APPEND ui_srcs "bt_ui/bt_ui.c"
         "bt_ui/fonts/lv_font_notosanssc_regular_28.c"
         "bt_ui/fonts/lv_font_notosansjk_regular_28.c")
    if(CONFIG_GMF_EXAMPLE_AUDIO_TECH_LE)
        list(APPEND ui_srcs "bt_ui/images/cis_stream_icon.c"
         "bt_ui/images/bis_stream_icon.c"
         "bt_ui/images/speakers_dark_dark_small.c"
         "bt_ui/images/speakers_dark_light_small.c"
         "bt_ui/images/speakers_light_dark_small.c")
    endif()
endif()

set(reqs "bt" "nvs_flash" "console" "esp_board_manager" "esp_codec_dev" "esp_lvgl_port" "lvgl" "esp_jpeg")

idf_component_register(SRCS "main.c" "pool_reg.c" "cmd_reg.c" "stream_proc.c"
                    "bt_debug.c" "bt_debug_policy.c" "bt_debug_storage.c"
                    "bt_power.c" "bt_power_sequence.c"
                    ${ui_srcs}
                    INCLUDE_DIRS "." "bt_ui"
                    REQUIRES ${reqs}
                    )

if(CONFIG_BT_BLUEDROID_ENABLED AND CONFIG_GMF_EXAMPLE_A2DP_SINK
   AND NOT CONFIG_GMF_EXAMPLE_HFP_HF AND NOT CONFIG_GMF_EXAMPLE_PBAP_PCE
   AND NOT CONFIG_GMF_EXAMPLE_AUDIO_TECH_LE)
    foreach(api esp_a2d_register_callback esp_avrc_ct_register_callback esp_avrc_tg_register_callback
                esp_a2d_sink_init esp_avrc_ct_init esp_avrc_tg_init)
        target_link_options(${COMPONENT_LIB} INTERFACE "LINKER:--wrap=${api}")
    endforeach()
endif()

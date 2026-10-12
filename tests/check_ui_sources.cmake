cmake_minimum_required(VERSION 3.16)

# Exercise the real component source selection without requiring an IDF build.
set(CONFIG_EXAMPLE_BT_UI_ENABLE "${TEST_UI}")
set(CONFIG_GMF_EXAMPLE_AUDIO_TECH_LE OFF)
get_filename_component(demo_main "${CMAKE_CURRENT_LIST_DIR}/../demo/a2dp_sink/main" ABSOLUTE)

function(idf_component_register)
    cmake_parse_arguments(component "" "" "SRCS;INCLUDE_DIRS;REQUIRES" ${ARGN})
    foreach(source IN LISTS component_SRCS)
        if(NOT EXISTS "${demo_main}/${source}")
            message(FATAL_ERROR "Selected source does not exist: ${source}")
        endif()
    endforeach()
    foreach(source bt_power.c bt_power_sequence.c)
        if(NOT "${source}" IN_LIST component_SRCS)
            message(FATAL_ERROR "Both profiles must compile power backend: ${source}")
        endif()
    endforeach()
    if(TEST_UI AND NOT "bt_ui/bt_ui.c" IN_LIST component_SRCS)
        message(FATAL_ERROR "Screen profile must compile the official bt_ui")
    endif()
    if(NOT TEST_UI AND "bt_ui/bt_ui.c" IN_LIST component_SRCS)
        message(FATAL_ERROR "Headless profile must not compile bt_ui")
    endif()
endfunction()

include("${demo_main}/CMakeLists.txt")
# Structural guard only: the backend's fake-SDK tests do not exercise GMF reads.
file(READ "${demo_main}/stream_proc.c" stream_source)
foreach(required
        "esp_gmf_pipeline_get_head_el(bt2codec_pipe, &decoder)"
        "esp_gmf_port_set_wait_ticks(ESP_GMF_ELEMENT_GET(decoder)->in, pdMS_TO_TICKS(50))")
    string(FIND "${stream_source}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Bluetooth reader must use a finite stop-safe timeout: ${required}")
    endif()
endforeach()
message(STATUS "Classic UI=${TEST_UI}: selected sources exist")

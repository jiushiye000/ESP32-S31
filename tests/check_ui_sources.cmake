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
    if(TEST_UI AND NOT "bt_ui/bt_ui.c" IN_LIST component_SRCS)
        message(FATAL_ERROR "Screen profile must compile the official bt_ui")
    endif()
    if(NOT TEST_UI AND "bt_ui/bt_ui.c" IN_LIST component_SRCS)
        message(FATAL_ERROR "Headless profile must not compile bt_ui")
    endif()
endfunction()

include("${demo_main}/CMakeLists.txt")
message(STATUS "Classic UI=${TEST_UI}: selected sources exist")

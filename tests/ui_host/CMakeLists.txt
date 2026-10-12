cmake_minimum_required(VERSION 3.16)
project(bt_ui_host C CXX)
get_filename_component(repo "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(lvgl_dir "${repo}/demo/a2dp_sink/managed_components/lvgl__lvgl")
set(jpeg_dir "${repo}/demo/a2dp_sink/managed_components/espressif__esp_jpeg")
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_LIST_DIR}/lv_conf.h" CACHE STRING "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory("${lvgl_dir}" lvgl EXCLUDE_FROM_ALL)
file(GLOB font_sources "${repo}/demo/a2dp_sink/main/bt_ui/fonts/*.c")
add_executable(test_ui test_ui.c platform.c
    "${repo}/demo/a2dp_sink/main/bt_ui/bt_ui.c"
    "${repo}/demo/a2dp_sink/main/bt_debug_policy.c" ${font_sources})
target_include_directories(test_ui PRIVATE stubs "${repo}/demo/a2dp_sink/main"
    "${repo}/demo/a2dp_sink/main/bt_ui" "${jpeg_dir}/include")
target_compile_options(test_ui PRIVATE -g --coverage -ffunction-sections -fdata-sections -Wall -Wextra)
target_link_options(test_ui PRIVATE --coverage -Wl,--gc-sections)
target_link_libraries(test_ui PRIVATE lvgl m)
add_executable(test_ui_jpeg test_ui_jpeg.c platform.c
    "${repo}/demo/a2dp_sink/main/bt_ui/bt_ui.c"
    "${repo}/demo/a2dp_sink/main/bt_debug_policy.c" ${font_sources}
    "${jpeg_dir}/jpeg_decoder.c" "${jpeg_dir}/tjpgd/tjpgd.c")
target_include_directories(test_ui_jpeg PRIVATE stubs "${repo}/demo/a2dp_sink/main"
    "${repo}/demo/a2dp_sink/main/bt_ui" "${jpeg_dir}/include" "${jpeg_dir}/tjpgd")
target_compile_definitions(test_ui_jpeg PRIVATE UI_HOST_REAL_JPEG=1)
target_compile_options(test_ui_jpeg PRIVATE -g --coverage -ffunction-sections -fdata-sections -Wall -Wextra)
target_link_options(test_ui_jpeg PRIVATE --coverage -Wl,--gc-sections)
target_link_libraries(test_ui_jpeg PRIVATE lvgl m)
option(UI_HOST_SANITIZERS "Check app buffer accesses and undefined behavior" OFF)
if(UI_HOST_SANITIZERS)
    foreach(target test_ui test_ui_jpeg)
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endforeach()
endif()

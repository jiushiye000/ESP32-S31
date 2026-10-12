#!/usr/bin/env bash

set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
demo_dir="${root_dir}/demo/a2dp_sink"
config_file="${demo_dir}/sdkconfig.defaults.korvo1"

required_files=(
    "${root_dir}/README.md"
    "${root_dir}/scripts/build_demo.sh"
    "${root_dir}/scripts/flash_demo.sh"
    "${demo_dir}/CMakeLists.txt"
    "${demo_dir}/main/CMakeLists.txt"
    "${demo_dir}/main/main.c"
    "${demo_dir}/sdkconfig.defaults.korvo1.ui"
    "${demo_dir}/sdkconfig.defaults.debug"
    "${config_file}"
)

for path in "${required_files[@]}"; do
    if [[ ! -f "${path}" ]]; then
        printf 'missing required file: %s\n' "${path}" >&2
        exit 1
    fi
done

required_config_lines=(
    'CONFIG_BT_ENABLED=y'
    'CONFIG_BT_BLUEDROID_ENABLED=y'
    'CONFIG_BT_CLASSIC_ENABLED=y'
    'CONFIG_BT_A2DP_ENABLE=y'
    'CONFIG_BT_A2DP_USE_EXTERNAL_CODEC=y'
    'CONFIG_BT_AVRCP_ENABLED=y'
    'CONFIG_GMF_EXAMPLE_AUDIO_TECH_CLASSIC=y'
    'CONFIG_GMF_EXAMPLE_A2DP_SINK=y'
    '# CONFIG_GMF_EXAMPLE_HFP_HF is not set'
    '# CONFIG_GMF_EXAMPLE_AUDIO_TECH_LE is not set'
    '# CONFIG_EXAMPLE_BT_UI_ENABLE is not set'
)

for expected in "${required_config_lines[@]}"; do
    if ! grep -Fqx "${expected}" "${config_file}"; then
        printf 'missing required config: %s\n' "${expected}" >&2
        exit 1
    fi
done

if ! grep -Fq 'esp32_s31_korvo_1' "${root_dir}/scripts/build_demo.sh"; then
    printf 'build script does not select esp32_s31_korvo_1\n' >&2
    exit 1
fi

for expected in 'CONFIG_EXAMPLE_BT_UI_ENABLE=y' 'CONFIG_LOG_MAXIMUM_LEVEL_DEBUG=y'; do
    if ! grep -Fqx "${expected}" "${demo_dir}/sdkconfig.defaults.korvo1.ui"; then
        printf 'missing UI profile config: %s\n' "${expected}" >&2
        exit 1
    fi
done

for module in HCI BTM L2CAP SDP GAP A2D AVDT AVCT AVRC APPL BTIF BTC OSI; do
    expected="CONFIG_BT_LOG_${module}_TRACE_LEVEL_DEBUG=y"
    if ! grep -Fqx "${expected}" "${demo_dir}/sdkconfig.defaults.debug"; then
        printf 'missing retained stack trace: %s\n' "${expected}" >&2
        exit 1
    fi
done
for expected in 'CONFIG_LOG_MAXIMUM_LEVEL_DEBUG=y' 'CONFIG_LOG_DEFAULT_LEVEL_INFO=y' 'CONFIG_LOG_DYNAMIC_LEVEL_CONTROL=y'; do
    if ! grep -Fqx "${expected}" "${demo_dir}/sdkconfig.defaults.debug"; then
        printf 'missing debug profile config: %s\n' "${expected}" >&2
        exit 1
    fi
done

cmake -DTEST_UI=OFF -P "${root_dir}/tests/check_ui_sources.cmake"
cmake -DTEST_UI=ON -P "${root_dir}/tests/check_ui_sources.cmake"
for ui_variant in OFF ON; do
    for le_variant in OFF ON; do
        cmake -DTEST_UI="${ui_variant}" -DTEST_LE="${le_variant}" -P "${root_dir}/tests/check_ui_pages.cmake"
    done
done
bash "${root_dir}/tests/test_debug.sh"
bash "${root_dir}/tests/test_debug_component.sh"
bash "${root_dir}/tests/test_bt_power.sh"

printf 'demo structure and configuration checks passed\n'

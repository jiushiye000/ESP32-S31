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

printf 'demo structure and configuration checks passed\n'

#!/usr/bin/env bash

set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
demo_dir="${root_dir}/demo/a2dp_sink"

# shellcheck disable=SC1090
source "${root_dir}/scripts/idf_env.sh"

idf=("${idf_python}" "${IDF_PATH}/tools/idf.py" --preview)

cd "${demo_dir}"
export SDKCONFIG_DEFAULTS="sdkconfig.defaults.korvo1"
board_defaults="components/gen_bmgr_codes/board_manager.defaults"

if [[ ! -f sdkconfig ]] || ! grep -Fq 'CONFIG_IDF_TARGET_ESP32S31=y' sdkconfig; then
    "${idf[@]}" set-target esp32s31
fi

if [[ "${REGENERATE_BOARD:-0}" == "1" ]] ||
    [[ ! -f "${board_defaults}" ]] ||
    ! grep -Fq 'CONFIG_ESP_BOARD_NAME="esp32_s31_korvo_1"' "${board_defaults}"; then
    "${idf[@]}" bmgr -b esp32_s31_korvo_1
fi

"${idf[@]}" build

printf '\nDemo build completed: %s\n' "${demo_dir}/build"

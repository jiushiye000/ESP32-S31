#!/usr/bin/env bash

set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
demo_dir="${root_dir}/demo/a2dp_sink"

case "${DEMO_UI:-0}" in
    0) build_dir="build"; config_path="sdkconfig"; defaults="sdkconfig.defaults.korvo1" ;;
    1) build_dir="build-ui"; config_path="sdkconfig.ui"; defaults="sdkconfig.defaults.korvo1;sdkconfig.defaults.korvo1.ui" ;;
    *) printf 'DEMO_UI must be 0 (headless) or 1 (screen).\n' >&2; exit 1 ;;
esac

# shellcheck disable=SC1090
source "${root_dir}/scripts/idf_env.sh"

idf=("${idf_python}" "${IDF_PATH}/tools/idf.py" --preview
     -B "${demo_dir}/${build_dir}" -D "SDKCONFIG=${demo_dir}/${config_path}")

cd "${demo_dir}"
board_defaults="components/gen_bmgr_codes/board_manager.defaults"
board_defaults_present=0
if [[ -f "${board_defaults}" ]]; then
    board_defaults_present=1
    defaults="${defaults};${board_defaults}"
fi
export SDKCONFIG_DEFAULTS="${defaults}"
idf+=(-D "SDKCONFIG_DEFAULTS=${defaults}")

new_config=0
if [[ ! -f "${config_path}" ]] || ! grep -Fq 'CONFIG_IDF_TARGET_ESP32S31=y' "${config_path}"; then
    new_config=1
    "${idf[@]}" set-target esp32s31
fi

if [[ "${REGENERATE_BOARD:-0}" == "1" ]] ||
    [[ ! -f "${board_defaults}" ]] ||
    ! grep -Fq 'CONFIG_ESP_BOARD_NAME="esp32_s31_korvo_1"' "${board_defaults}"; then
    "${idf[@]}" bmgr -b esp32_s31_korvo_1
fi

if [[ "${board_defaults_present}" == "0" ]]; then
    defaults="${defaults};${board_defaults}"
    export SDKCONFIG_DEFAULTS="${defaults}"
    idf+=(-D "SDKCONFIG_DEFAULTS=${defaults}")
    # The first set-target config predates the generated board defaults.
    # Preserve that generated file, then let IDF load the complete defaults.
    if [[ "${new_config}" == "1" && -f "${config_path}" ]]; then
        config_backup="$(mktemp "${config_path}.before-board.XXXXXX")"
        mv "${config_path}" "${config_backup}"
    fi
fi

"${idf[@]}" reconfigure
for required in 'CONFIG_ESP_BOARD_DEV_AUDIO_CODEC_SUPPORT=y' 'CONFIG_CODEC_ES8389_SUPPORT=y'; do
    if ! grep -Fqx "${required}" "${config_path}"; then
        printf 'Invalid board config: missing %s in %s. Preserve it and regenerate from defaults.\n' "${required}" "${config_path}" >&2
        exit 1
    fi
done
if [[ "${DEMO_UI:-0}" == "1" ]]; then
    for required in 'CONFIG_EXAMPLE_BT_UI_ENABLE=y' 'CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUPPORT=y' 'CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_RGB_SUPPORT=y' 'CONFIG_ESP_BOARD_DEV_LCD_TOUCH_SUPPORT=y' 'CONFIG_ESP_BOARD_DEV_LCD_TOUCH_SUB_I2C_SUPPORT=y' 'CONFIG_LOG_MAXIMUM_LEVEL_DEBUG=y'; do
        if ! grep -Fqx "${required}" "${config_path}"; then
            printf 'Invalid UI config: missing %s in %s. Preserve it and regenerate from defaults.\n' "${required}" "${config_path}" >&2
            exit 1
        fi
    done
fi

"${idf[@]}" build

printf '\nDemo build completed: %s\n' "${demo_dir}/${build_dir}"

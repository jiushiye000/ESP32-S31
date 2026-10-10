#!/usr/bin/env bash

# This file is sourced by the build and flash scripts.

activation_script="${IDF_ACTIVATE_SCRIPT:-/home/yiyuan/.espressif/tools/activate_idf_v6.1.sh}"

if [[ ! -x "${activation_script}" ]]; then
    printf 'ESP-IDF activation script not found: %s\n' "${activation_script}" >&2
    return 1
fi

idf_env="$("${activation_script}" -e)"
eval "${idf_env}"
PATH="${PATH}:${SYSTEM_PATH}"

export PATH
export ESP_IDF_VERSION
export IDF_TOOLS_PATH
export IDF_COMPONENT_LOCAL_STORAGE_URL
export IDF_PATH
export ESP_ROM_ELF_DIR
export OPENOCD_SCRIPTS
export IDF_PYTHON_ENV_PATH

if [[ -z "${IDF_PATH:-}" || ! -f "${IDF_PATH}/tools/idf.py" ]]; then
    printf 'ESP-IDF v6.1 environment was not exported correctly\n' >&2
    return 1
fi

idf_python="${IDF_PYTHON_ENV_PATH}/bin/python"
if [[ ! -x "${idf_python}" ]]; then
    printf 'ESP-IDF Python was not found: %s\n' "${idf_python}" >&2
    return 1
fi

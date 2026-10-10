#!/usr/bin/env bash

set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
demo_dir="${root_dir}/demo/a2dp_sink"

case "${DEMO_UI:-0}" in
    0) build_dir="build"; config_path="sdkconfig" ;;
    1) build_dir="build-ui"; config_path="sdkconfig.ui" ;;
    *) printf 'DEMO_UI must be 0 (headless) or 1 (screen).\n' >&2; exit 1 ;;
esac

"${root_dir}/scripts/build_demo.sh"

# shellcheck disable=SC1090
source "${root_dir}/scripts/idf_env.sh"

idf_python="${IDF_PYTHON_ENV_PATH}/bin/python"
idf=("${idf_python}" "${IDF_PATH}/tools/idf.py" --preview
     -B "${demo_dir}/${build_dir}" -D "SDKCONFIG=${demo_dir}/${config_path}")

port="${1:-}"
if [[ -z "${port}" ]]; then
    shopt -s nullglob
    candidates=(/dev/ttyACM* /dev/ttyUSB*)
    shopt -u nullglob
    if [[ ${#candidates[@]} -ne 1 ]]; then
        printf 'Expected exactly one serial device, found %d.\n' "${#candidates[@]}" >&2
        printf 'Usage: %s /dev/ttyUSB0\n' "$0" >&2
        exit 1
    fi
    port="${candidates[0]}"
fi

cd "${demo_dir}"
"${idf[@]}" -p "${port}" flash monitor

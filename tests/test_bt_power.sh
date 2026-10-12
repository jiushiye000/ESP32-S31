#!/usr/bin/env bash
set -euo pipefail
root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
task_power_build="$(mktemp -d /tmp/esp32-power-host.XXXXXX)"
cc -std=c11 -Wall -Wextra -Werror --coverage \
    -I "${root_dir}/demo/a2dp_sink/main" \
    "${root_dir}/tests/test_bt_power.c" \
    "${root_dir}/demo/a2dp_sink/main/bt_power_sequence.c" \
    -o "${task_power_build}/test_bt_power"
"${task_power_build}/test_bt_power"
cc -std=c11 -Wall -Wextra -Werror -Wno-unused-variable --coverage \
    -I "${root_dir}/tests/power_host" -I "${root_dir}/demo/a2dp_sink/main" \
    "${root_dir}/tests/power_host/test_backend.c" \
    "${root_dir}/demo/a2dp_sink/main/bt_power_sequence.c" \
    -o "${task_power_build}/test_backend"
"${task_power_build}/test_backend"
cc -std=c11 -Wall -Wextra -Werror --coverage \
    -I "${root_dir}/tests/power_host" -I "${root_dir}/demo/a2dp_sink/main" \
    "${root_dir}/tests/power_host/test_unavailable.c" \
    -o "${task_power_build}/test_unavailable"
"${task_power_build}/test_unavailable"
cd "${task_power_build}"
gcov -b test_backend-test_backend.gcno
gcov -b -o "${task_power_build}/test_bt_power-bt_power_sequence.gcno" \
    "${root_dir}/demo/a2dp_sink/main/bt_power_sequence.c"
printf 'BT power host artifacts: %s\n' "${task_power_build}"

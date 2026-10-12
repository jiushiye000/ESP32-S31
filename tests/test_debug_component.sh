#!/usr/bin/env bash
set -euo pipefail
root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
for task_ui in OFF ON; do
    cmake -DTEST_UI="${task_ui}" -P "${root_dir}/tests/check_debug_component.cmake"
done
task_debug_build="$(mktemp -d /tmp/esp32-debug-component.XXXXXX)"
cc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror --coverage \
    -I "${root_dir}/tests/debug_console_host" -I "${root_dir}/components/bt_debug/include" \
    "${root_dir}/tests/debug_console_host/test_console.c" \
    "${root_dir}/components/bt_debug/src/bt_debug_console.c" \
    "${root_dir}/components/bt_debug/src/bt_debug_policy.c" \
    -o "${task_debug_build}/test_console"
"${task_debug_build}/test_console"
cd "${task_debug_build}"
gcov -b test_console-bt_debug_console.gcno
printf 'Debug component host artifacts: %s\n' "${task_debug_build}"

#!/usr/bin/env bash
set -euo pipefail
root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
task_tmp="$(mktemp -d /tmp/esp32-debug-tests.XXXXXX)"
printf 'Debug host test artifacts: %s\n' "${task_tmp}"
for test_name in policy storage; do
    gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror --coverage \
        -I "${root_dir}/demo/a2dp_sink/main" \
        "${root_dir}/tests/test_debug_${test_name}.c" \
        "${root_dir}/demo/a2dp_sink/main/bt_debug_${test_name}.c" \
        -o "${task_tmp}/test_${test_name}"
    "${task_tmp}/test_${test_name}" "${task_tmp}"
done
cd "${task_tmp}"
gcov -b test_policy-bt_debug_policy.gcno test_storage-bt_debug_storage.gcno

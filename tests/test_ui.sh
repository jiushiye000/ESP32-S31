#!/usr/bin/env bash
set -euo pipefail
root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
task_ui_build="${UI_HOST_BUILD:-/tmp/esp32-ui-host-20261012}"
cmake -S "${root_dir}/tests/ui_host" -B "${task_ui_build}" -G Ninja \
    -D "UI_HOST_SANITIZERS=${UI_HOST_SANITIZERS:-OFF}" > "${task_ui_build}.configure.log"
cmake --build "${task_ui_build}" -j 8 > "${task_ui_build}.build.log" 2>&1
failed=0
for test_case in contrast play play_reject fonts cover_clear decode_lock cover_stale cover_decode_stale cover_failure cover_bounds cover_queue disconnect utf8 antitear media_layout; do
    if ! "${task_ui_build}/test_ui" "${test_case}"; then failed=1; fi
done
printf 'UI host artifacts: %s\n' "${task_ui_build}"
exit "${failed}"

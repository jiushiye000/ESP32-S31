#!/usr/bin/env bash
# Regenerate only the static supplemental font. Not run by firmware builds.
set -euo pipefail
root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
task_font_dir="$(mktemp -d /tmp/esp32-font.XXXXXX)"
font_file="${task_font_dir}/NotoSansCJKkr-Regular.otf"
curl -fL --max-time 60 -o "${font_file}" \
    'https://raw.githubusercontent.com/notofonts/noto-cjk/f8d157532fbfaeda587e826d4cd5b21a49186f7c/Sans/OTF/Korean/NotoSansCJKkr-Regular.otf'
printf '6bcb2a0703aa137e874fc2dffa85f6c21ba9a67fa329e81b8c801663af7e992a  %s\n' "${font_file}" | sha256sum -c -
npx --yes --package=lv_font_conv@1.5.3 lv_font_conv \
    --font "${font_file}" \
    --range 0x3040-0x30FF,0x31F0-0x31FF,0x1100-0x11FF,0x3130-0x318F,0xAC00-0xD7A3,0xA960-0xA97F,0xD7B0-0xD7FF \
    --size 28 --bpp 1 --no-compress --format lvgl --lv-include lvgl.h \
    --lv-font-name lv_font_notosansjk_regular_28 \
    --output "${root_dir}/demo/a2dp_sink/main/bt_ui/fonts/lv_font_notosansjk_regular_28.c"
# The official converter adds an empty EOF line; normalize generated whitespace.
sed -i '${/^$/d;}' "${root_dir}/demo/a2dp_sink/main/bt_ui/fonts/lv_font_notosansjk_regular_28.c"
printf 'Source font preserved in %s\n' "${task_font_dir}"

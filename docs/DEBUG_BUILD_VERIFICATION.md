# 模块 Debug 框架验证记录

日期：2026-10-10。仅本机源码、host 测试和固件构建验证，未烧录、未连接串口或操作板子。

## 本轮范围

实现模块级 DEBUG、屏幕/CLI 统一控制、NVS 设置和有界 SD 文本日志；去掉 Debug 页
历史日志列表。保留 A2DP Sink / AVRCP，未开启 HFP、LE Audio、Cover Art。
未修改 IDF、GMF 子模块、managed components、manifest 或锁文件。
截至本报告（2026-10-10），屏幕抗撕裂及 Media 三项改造尚未实施。
后续 2026-10-12 的实现与验证见 [显示与 Media 指南](MEDIA_DISPLAY_GUIDE.md)。

## 测试与审查

- 初始 RED：缺少 policy/storage 实现时 `tests/test_debug.sh` 失败，本地检查点 `21b28de`。
- 补充 RED：stdlib flush 成功但 filesystem sync 失败的契约测试失败，检查点 `951fc29`。
- GREEN：`bash tests/validate_demo.sh` 成功，两模式源码选择、Debug defaults 和核心测试通过。
- `gcc -Wall -Wextra -Werror` 通过；host AddressSanitizer + UndefinedBehaviorSanitizer 通过。
- policy 行覆盖率 100%，storage 95.71%，合计 97.30%；storage branch taken 91.18%。
- 覆盖独立开关、OFF 基线、能力门控、NVS mask 清洗、逐包 opt-in；存储覆盖无卡、
  目录错误、文件碰撞不覆盖、轮转/会话上限、write/flush/fsync/close 失败。
- 覆盖率仅指两个 portable C 核心，不含 ESP/FreeRTOS/NVS adapter、LVGL 或真实 SD。
- 只读代码审查通过；修正慢串口生产者在 OFF/ON 后错入新会话的 epoch 边界。
- `bash -n`、非法 DEMO_UI 拒绝、`git diff --check` 通过。

覆盖率测试产物：`/tmp/esp32-debug-tests.hhgCRm/`；sanitizer：
`/tmp/esp32-debug-sanitizers.NoUim4/`。这些为临时目录，不是长期测试结果存档。

## 固件构建

```bash
DEMO_UI=1 ./scripts/build_demo.sh
# 最后两处 backend/registry 修订后，沿用已配置的 UI 构建增量编译：
source scripts/idf_env.sh
cmake --build demo/a2dp_sink/build-ui
# 缩短本次 registry API 超时，不改变项目配置或依赖：
IDF_COMPONENT_API_TIMEOUT=3 DEMO_UI=0 ./scripts/build_demo.sh
```

两种构建成功，分区检查通过；esptool 5.4.0 的 checksum / validation hash 均有效。
UI 初次构建 2678 步、最终增量 10 步；无屏最终构建 2676 步。

| 项目 | 无屏 | 屏幕 |
| --- | --- | --- |
| 配置 / 目录 | sdkconfig / build | sdkconfig.ui / build-ui |
| 默认 ESP 日志 | INFO=3 | INFO=3 |
| 编译 ESP 上限 | DEBUG=4 | DEBUG=4 |
| 13 个栈模块保留 trace | DEBUG=5 | DEBUG=5 |
| bt_audio.bin | 2,976,208 bytes | 5,463,536 bytes |
| 8 MiB 应用分区剩余 | 65% | 35% |

旧配置备份在 `/tmp/esp32-debug-config.Xa4DeZ/`；只对生成的 sdkconfig 做了机械
日志 selector 迁移，其他硬件/音频参数保留。`sdkconfig.defaults.debug` 与构建守卫
保证新生成配置保留实际栈 DEBUG，旧 WARNING 配置会明确报错而非产生无效开关。

产物 SHA256：

```text
build/bt_audio.bin    6d827f3f270f589547824f12eb59758385f9f4f06056092e77a56f01f3d1615b
build-ui/bt_audio.bin c23408e3ee35cd68b068345b5c047f907478e989c681f583613ba5b8a94c5083
```

UI image App version 为 `21b28de-dirty`，无屏为 `951fc29-dirty`，因为配置/构建在
GREEN 提交前执行；最终增量构建未重新生成 UI 的 app-version 元数据。
镜像对应本轮最终代码，但版本字符串不是最终提交号。

完整命令日志：`/tmp/esp32-debug-build-ui.log`、
`/tmp/esp32-debug-build-ui-final.log`、`/tmp/esp32-debug-build-headless.log`。
registry 暂不可达时组件管理器沿用已安装的锁定依赖；没有升级依赖。
保留上游 CMake/Kconfig 提示及原两个 unused-function 警告，没有新增 Debug 编译警告。

## 未验证边界

没有真实触摸、NVS 重启恢复、SD 卡文件、热拔插、长时音频共存或撕裂改善结果。
没有保证日志无丢失、零时序影响或断电安全。新克隆首次 defaults 生成路径未重新
在空缓存环境验证。上板验收见 [Debug 指南](DEBUG_GUIDE.md)。
本轮代理只执行本地检查点提交，没有执行 push 或任何远端写操作。

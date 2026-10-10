# 屏幕版与无屏版编译验证

此页保留首次 UI 迭代的历史结果，不代表当前 Debug 框架产物；本轮结果见
[Debug 编译验证](DEBUG_BUILD_VERIFICATION.md)。

验证日期：2026-10-10。此记录是本机验证，不是新 UI 的上板运行报告。

## 版本与修改边界

- ESP-IDF：本机 v6.1，ESP32-S31 preview，GCC 15.2.0。
- ESP-GMF：v1.0 / `efc94f51c22d3af68a6f2f654f0eec9d13a46cb3`。
- Board Manager 0.7.3~1、esp_lvgl_port 2.9.0、LVGL 9.5.0。
- 原 `dependencies.lock`、组件 manifest、基础 Korvo-1 defaults 和第三方源码未修改。
- A2DP Sink / AVRCP 保持开启；未增加 HFP、LE Audio 或 Cover Art。

## 执行结果

```bash
bash tests/validate_demo.sh
bash -n scripts/idf_env.sh scripts/build_demo.sh scripts/flash_demo.sh tests/validate_demo.sh
DEMO_UI=1 bash scripts/build_demo.sh
bash scripts/build_demo.sh
git diff --check
```

上述命令均成功。UI 全量构建完成 2708 个步骤，之后对最终代码重新增量构建；
无屏版最终构建完成 2672 个步骤。两种构建均出现 `Project build complete`。
无屏/屏幕 CMake 源文件选择检查、非法 DEMO_UI 拒绝检查、Git 忽略检查通过。
代码审查发现的板级 defaults 加载、声道位图计数和事件历史溢出状态问题已修正。

| 项目 | 无屏版 | 屏幕版 |
| --- | --- | --- |
| 配置 | sdkconfig | sdkconfig.ui |
| 目录 | build | build-ui |
| LVGL UI | 关闭 | 开启 |
| 默认日志级别 | INFO / 3 | INFO / 3 |
| 编译日志上限 | INFO / 3 | DEBUG / 4 |
| bt_audio.bin 大小 | 2,808,352 bytes | 5,440,544 bytes |
| 8 MiB 应用分区剩余 | 67% | 35% |

检查最终配置：屏幕版的 Audio Codec、ES8389、RGB LCD、LCD Touch、I2C Touch
均开启，目标为 esp32s31，Flash 为 16MB。esptool 5.4.0 对两种镜像的 checksum
和 validation hash 检查均有效。

最终文件 SHA256：

```text
build-ui/bt_audio.bin e2804ef4ae3618c9b8ca85811beb98c183fa9c68fb88f4165f1370c0d3826ca6
build/bt_audio.bin    a337fbd2d9f1ce36110cb80386c0129ac526fb8575b293d0cad053d4acc07be0
```

固件在本地 GREEN 检查点提交前构建，App version 为 `561c728-dirty`；这是本次
源码变更尚未提交时生成的版本信息，不表示子模块被修改。

最终命令完整日志：

- `/tmp/esp32-ui-verify.6vP9Bh.log`
- `/tmp/esp32-headless-verify.rO0OD0.log`

全量构建保留原例程的两个 unused-function 警告以及上游 CMake/Kconfig 提示，
无编译错误。首次 UI 失败构建及其配置已保留在
`/tmp/esp32-ui-first-build.yxZWml/`，未删除原无屏配置。

## 未覆盖范围

未烧录、未操作串口或开发板，没有新 UI 的实际显示、触摸、音频共存和长时间
运行结果；未运行硬件 E2E 或测量 GUI 覆盖率。首次克隆后自动生成板级 defaults
的分支经过代码审查，但未在另一份全新克隆上运行。
上板项目见 [UI_GUIDE.md](UI_GUIDE.md)。所有修改和检查点提交仅在本地，不推送。

# 屏幕版：复用官方 LVGL UI

## 复用边界

没有更换 SDK、升级依赖或另写 LCD/触摸驱动。继续使用仓库锁定的
ESP-GMF v1.0、Board Manager 0.7.3~1、esp_lvgl_port 2.9.0 和 LVGL 9.5.0。

- Board Manager 初始化 Korvo-1 的 RGB LCD 和 GT1151 触摸，提供现成句柄。
- `bt_ui_init()` 使用官方 `lvgl_port_add_disp_rgb()`、`lvgl_port_add_touch()`。
- `bt_ui_media_create()` 是原例程播放器，继续提供歌曲信息和 AVRCP 控制。
- Debug tab 只用 LVGL 自带的 tabview、label、button、switch、flex 和 timer。
- 原媒体/封面更新沿用官方 LVGL port 锁；新诊断状态采用短临界区快照和
  LVGL timer 每 500 ms 刷新模块开关与存储状态，未增加第二个 LVGL 调度任务。

官方入口：

- [ESP-GMF v1.0 bt_audio 示例](https://github.com/espressif/esp-gmf/blob/v1.0/packages/esp_bt_audio/examples/bt_audio/README.md)
- [esp_lvgl_port 2.9.0 使用说明](https://components.espressif.com/components/espressif/esp_lvgl_port/versions/2.9.0/readme)
- [Board Manager LCD](https://docs.espressif.com/projects/esp-board-manager/en/latest/references/devices/display-lcd.html)
- [Board Manager Touch](https://docs.espressif.com/projects/esp-board-manager/en/latest/references/devices/lcd-touch.html)
- [ESP-IDF v6.1 日志](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s31/api-reference/system/log.html)

## 构建与配置

```bash
cd /home/yiyuan/share/esp32
./tests/validate_demo.sh
DEMO_UI=1 ./scripts/build_demo.sh
```

屏幕版使用 `build-ui/` 与 `sdkconfig.ui`，默认无屏版仍使用 `build/` 和
`sdkconfig`。SDK defaults 是基础音频配置、Debug overlay、UI overlay 与生成的官方板级
defaults；Board Manager 0.7.3~1 的 CLI 辅助回调只识别默认 `sdkconfig`，
因此脚本显式把板级文件传给 IDF，并检查音频、LCD、RGB、触摸等支持。

`sdkconfig.defaults.*` 是首次生成配置的默认值，不能覆盖已保存在 sdkconfig 中
的显式值。若手工改坏 `sdkconfig.ui`，脚本会报错，不能靠修改 defaults 自动修复。
先保留现有配置，再重新生成，例如：

```bash
# 临时目录保存你的旧配置，不删除它。
ui_config_backup=$(mktemp -d /tmp/esp32-ui-config.XXXXXX)
mv demo/a2dp_sink/sdkconfig.ui "$ui_config_backup/sdkconfig.ui"
DEMO_UI=1 ./scripts/build_demo.sh
```

首次克隆若尚无生成的 Board Manager defaults，脚本先生成板级代码，保留临时
目标配置为 `sdkconfig*.before-board.*`，再使用完整 defaults 生成最终配置。

编译成功后，由你确认串口与硬件再烧录：

```bash
DEMO_UI=1 ./scripts/flash_demo.sh /dev/ttyUSB0
```

屏幕版沿用官方 UI 命名逻辑，手机搜索 `GMF_BT_UI_XXXX`；无屏版仍是
`esp_classic_XXXX`。两个模式烧录同一应用分区，烧录一个会替换板上当前固件，
但本机配置和构建产物互相独立。

## 页面与日志

启动后默认打开 Debug 页，未连接时也可查看；点击 Media 切到官方播放器。
当前 A2DP Sink 基线没有 HFP，不显示拨号页；未启用 AVRCP Cover Art，
播放器显示占位封面，歌曲标题/歌手是否提供取决于手机播放器。

Debug 页现在是模块级控制面板，不在屏幕上滚动打印日志。A2DP、AVRCP、
AVDTP、AVCTP、HCI、L2CAP、GAP、SDP、OSI 等独立开关，未编译模块灰显；
下方显示连接/流/播放简要状态、SD 请求与实际存储状态、丢弃/截断计数及 NVS
保存状态。开关与串口 `debug` 命令共用同一后端，并自动保存到 NVS。

屏幕和无屏版都保留 ESP DEBUG=4 及 Bluedroid DEBUG trace=5，默认输出仍为
应用 INFO、协议栈 WARN；开启一个模块才提高该模块实际 tag。逐包日志独立
显式开启，不能当性能测量配置。详细使用、SD 边界和扩展方式见
[模块 Debug 指南](DEBUG_GUIDE.md)。STARTED / PLAYING 仍不能证明物理出声。

## 从现有代码学习 LVGL

建议依次阅读 `main/bt_ui/bt_ui.c` 中：

1. `bt_ui_init()`：理解 Board Manager 句柄与 LVGL port 的关系。
2. `bt_ui_create()`：理解父子对象、tabview、页面布局。
3. `bt_ui_media_create()`：观察 label、button、image、flex 与点击回调。
4. `debug_create()` / `debug_refresh()`：观察 switch、flex、timer 与状态刷新。
5. `main.c` 的 `bt_audio_event_cb()`：追踪蓝牙回调怎样更新界面。

先改一个 label 的文字或颜色，再调整布局，然后增加只读状态字段。
跨任务操作 LVGL 时沿用 `lvgl_port_lock/unlock`，不要另起线程调用
`lv_timer_handler()`，也不要在音频数据回调里逐包刷新屏幕。

## 上板回归（尚需硬件验证）

1. 不连接手机启动：Debug 页可见，触摸切换 Media/Debug，无重启。
2. 手机连接、断开和重连：简要状态正确，断开后仍可查看 Debug。
3. 播放/暂停/切歌：触摸按钮有效，手机通知更新状态，歌曲信息正常。
4. 输入 44.1 kHz 与 48 kHz SBC 文件：串口 Codec 信息正常，输出仍为基线 48 kHz。
5. 从手机调节音量：屏幕浮层与听感正常。
6. 按 [模块 Debug 指南](DEBUG_GUIDE.md) 验证独立开关、重启持久化、无卡与 SD。
7. 连续操作后断开：当前简要状态应显示最终断开状态。
8. 播放至少 10 分钟并反复切页：无音频明显卡顿、触摸卡死或异常重启。

本机结构校验与编译不能替代这些测试，也没有给出 GUI 运行覆盖率。
本轮两种构建的具体结果见 [Debug 编译验证](DEBUG_BUILD_VERIFICATION.md)，
首次 UI 迭代结果保留在 [历史编译验证](UI_BUILD_VERIFICATION.md)。

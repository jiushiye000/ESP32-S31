# 模块 Debug 控制与 SD 文本日志

本轮实现模块开关框架，不在屏幕上打印日志，不改 SDK/第三方组件源码，
也不增加 Bluetooth profile 角色。屏幕版和无屏版共用同一后端。

## 与 quec_bt.conf 对齐的部分

参考本机 `vendor/etc/bluetooth/quec_bt.conf` 的 `QUEC_BT_DEBUG_LEVEL_*` 分层，
按实际 ESP 标签建立 registry；没有加载 Android 配置，也不是移植 Quec 的 BAL/JNI。
目前是逐模块 ON/OFF，后续可在策略层扩展级别、实验配置与导入导出。

| 控制键 | 当前范围 |
| --- | --- |
| API | Demo 回调、公共音频 API |
| GAP / BTM | 发现、配对、连接、安全与链路管理 |
| HCI / L2CAP / SDP | Host HCI 文本、通道与服务发现 |
| A2D / AVDT / AVCT / AVRC | A2DP、AVDTP、AVCTP、AVRCP CT/TG |
| BAL / BTC / BTIF / OSI | BTA 适配、任务回调、接口、定时器/OS 适配 |
| AUDIO / UI | GMF 音频流程、项目 UI 与 LVGL 标签 |
| A2D_PACKETS | `BT_AUD_A2D_SINK` 的 DEBUG，含逐包信息，默认关闭 |
| HFP / BLE | 扩展占位，当前固件未编译，开关灰显且命令拒绝 |

OFF 不是关闭所有日志：协议栈保留 WARN/ERROR，应用保留 INFO/WARN/ERROR。
ON 将该模块的实际标签设为 ESP DEBUG=4。初始化会恢复 NVS 保存的模块开关，
首次启动默认全部 OFF。不要在音频时序/延迟测量中开启逐包 DEBUG。

旧开关只调整应用标签，而 Bluedroid trace 配置是 WARNING=2，不能恢复已裁掉的
协议栈 DEBUG。本轮通过 `sdkconfig.defaults.debug` 为 13 个现有栈模块保留
Bluedroid DEBUG trace=5；ESP log 编译上限为 DEBUG=4、默认仍 INFO=3。
这两套等级编号不同，不能照搬 Quec 数字。

A2D 和 A2D_PACKETS 分开：A2D 只提高 profile trace，Sink tag 仍 INFO。
由于上游逐包 DEBUG 和少量普通 DEBUG 共用 `BT_AUD_A2D_SINK`，关闭逐包选项
也会抑制该 tag 的普通 DEBUG；这不是按消息内容精确过滤。

## 操作与验证

Debug 页滚动列表中独立开启模块，底部显示简要状态、SD 状态和 NVS 保存状态。
串口控制完全相同，无屏版也可使用：

```text
debug status
debug A2D on
debug AVRC on
debug OSI on
debug A2D off
debug A2D_PACKETS on
debug all off
debug sd on
debug sd off
```

模块键区分大小写。开启时串口应出现 `DBG_CFG A2D=ON` 和
`D (...) BT_A2D: DBG_PROBE module=A2D runtime DEBUG active`；关闭后该模块
恢复基线，其他模块不变。探针证明运行时过滤已开放，不证明 profile 有业务流量。
然后实际连接、播放、暂停、断开，观察对应模块日志。

开关改变后异步合并保存 NVS，等显示 SAVED 后再重启；若显示 SAVE ERROR，
先用 `debug status` 读取错误，当前运行设置仍有效，不保证重启恢复。

高级 `log_level <tag|*> <level>` 命令仍保留。它可绕开模块策略，页面/CLI 将
偏离预期的标签显示为 RAW TAG OVERRIDE；下一次模块开关会重新应用整个 registry。
原始 tag 设置不会写入模块 NVS。VERBOSE 超过当前编译上限，仍拒绝。

## SD 保存

复用 Board Manager 的 `fs_sdcard` 已挂载句柄；不自行 mount、不格式化、不清空卡。
当前板配置挂载在 `/sdcard`，新文件形如：

```text
/sdcard/bt_logs/bt-<随机会话编号>-00.log
```

将卡在启动前插好，开启 `debug sd on`，检查状态 RECORDING 和实际路径。
关闭 `debug sd off` 并等状态 OFF 后取卡，再用电脑读文件。没有实现 USB 盘、
网络下载或热插拔重新挂载；运行中插卡后建议重启，不要录制时直接拔卡。

请求开启与实际记录分开：无卡时开关可为 ON，但显示 NO CARD / serial only，
不会自动无限重试。插卡重启或挂载成功后重新 OFF/ON。ERROR、LIMIT 都保留
串口输出；`debug status` 能读取文件路径、字节数、errno 与计数。

存储策略：

- hook 通过官方 `esp_log_set_vprintf()` 保留串口，同时格式化并零等待入队。
- 队列 64 块，每块最多 255 字节；超长块标记 `[TRUNCATED]` 并计数，队满丢弃。
- 单独低优先级 writer 负责文件 IO，约每 500 ms `fflush` + `fsync`。
- 单文件最多 4 MiB，单次启用最多 4 个文件；达到上限停止该会话，不覆盖旧文件。
- 每次重新启用创建新会话，文件排他创建；卡满/IO 错误停止录制，显示错误。
- dropped/truncated 为本次启动累计计数；bytes 是当前/最近会话已交给文件的字节数，
  不等于无丢失、断电安全或所有数据已经物理落盘。

这是 ESP 标准文本日志保存，不是 HCI BTSnoop。原始 printf/CLI 输出、ROM、
EARLY/ISR、初始化 hook 之前的启动输出和 Controller 二进制日志不保证收集。
writer 自身错误日志只到串口，不递归写卡。大量串口打印本身仍会扰动蓝牙时序，
异步文件队列只能避免在蓝牙回调中直接进行 SD 文件 IO，不能保证零影响。

官方依据：[ESP 日志 API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s31/api-reference/system/log.html)、
[FAT/VFS](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s31/api-reference/storage/fatfs.html)。

## 框架扩展点

- `bt_debug_policy.c`：模块/标签 registry 与策略，新增条目只能追加，避免改变 NVS bit 编号。
- `bt_debug.c`：ESP tag 适配、真实能力判断、NVS 与异步日志队列。
- `bt_debug_storage.c`：与 ESP/LVGL 无关的有界文件存储。
- `bt_ui.c`：根据 registry 自动生成开关，不直接持有文件或协议栈私有状态。

扩展新 profile 时，先开启真实角色与对应 compile-time traces，再补能力判断和
测试；不可只解除灰显。当前开关不控制配对策略、协议参数或 Controller 实验。

## 显示改造（2026-10-12 已实现，板端待验收）

抗撕裂已找到当前 `esp_lvgl_port 2.9.0` 的现成 RGB 方案：硬件 `num_fbs=2`、
PSRAM framebuffer、800×480 全屏 LVGL buffer、`direct_mode=true`、
`avoid_tearing=true`；由官方 port 完成 VSYNC 同步。不能只打开 direct_mode。
当前单硬件 framebuffer 与 LVGL double_buffer 并不等价。
两块 RGB565 framebuffer 约占 1.46 MiB PSRAM；先不混入 bounce-buffer 模式。
现通过 Board Manager 显式 amend 实施，保留官方 LCD 时序/引脚；仍需肉眼/音频共存验证。
参考[官方 RGB 示例](https://github.com/espressif/esp-bsp/blob/master/components/esp_lvgl_port/examples/rgb_lcd/main/main.c)。

Media 已启用官方 Cover Art、补日韩静态 fallback 字库、修正播放/暂停图标。
不要把占位图片当作已收到专辑封面；快速切歌的在途旧图仍有 GMF 回调身份限制。
实现、配置迁移和验收步骤见 [显示与 Media 指南](MEDIA_DISPLAY_GUIDE.md)。

## 上板验收清单

1. 无卡启动：UI、音频和串口可用；SD 开启后 NO CARD，不重启/卡死。
2. 分别开启 A2D / AVRC / OSI：探针和实际业务日志出现，关闭一项不影响其他项。
3. HFP / BLE 拒绝启用；All DEBUG off 不关闭 SD 保存。
4. 设置保存后重启：开关恢复，SD 请求恢复但无卡仍不得显示 RECORDING。
5. 有卡启动：实际文件产生，关闭后电脑能读取；既有文件内容保持不变。
6. 快速 SD OFF/ON：新路径/新会话，没有过期记录串入新会话。
7. 4 MiB 轮转、16 MiB 会话限制、卡满/IO 错误：状态正确且串口可继续。
8. 播放 10 分钟并切页/开关：无死锁、异常重启或不可接受的音频扰动。

Host 核心测试和固件编译不能替代上述硬件结果。

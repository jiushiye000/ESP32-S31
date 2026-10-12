# 手动蓝牙启停

本次只增加蓝牙生命周期控制，不改变分区、字体、LCD 驱动或 SDK/GMF 版本。
适配当前 Bluedroid + A2DP Sink + AVRCP CT/TG 配置；HFP、PBAP、LE Audio
组合尚未适配，显示 `UNAVAILABLE`，不尝试套用这套关闭流程。

## 使用

屏幕版增加 **System** 页，默认页仍为 Debug。`Bluetooth power` 开关提交异步
请求，状态显示 `ON / STOPPING / OFF / STARTING / ERROR`。STARTING/STOPPING
期间开关禁用；勾选状态来自后端记录的 Controller 状态，不是点击后的乐观结果。
状态中的 Step 和 Operation 给出当前步骤或失败位置，Request 是最近请求的返回码。

1. 切到 System，关闭 Bluetooth power。
2. 等待状态明确显示 `OFF`；手机会断开，屏幕、串口和 Debug/SD 日志设置继续工作。
3. 再开启，等待 `ON`，在手机端重新连接原设备并播放。
4. 出现 `ERROR` 时按 `Retry safe OFF`，只有完成 OFF 后才允许 ON。不要把
   Controller 显示 ENABLED 理解为 Host/Profile 已恢复正常。

屏幕版设备名仍为 `GMF_BT_UI_XXXX`，无屏版仍为 `esp_classic_XXXX`。关闭不删除
配对记录，不持久化电源开关；设备重启仍默认开启蓝牙。

两种构建都提供串口命令：

```text
bt_power status
bt_power off
bt_power status
bt_power on
```

请求返回 ESP_OK 只表示接受请求（或已经处于目标状态），完成情况以 status 为准。
蓝牙处于 OFF/busy/error 时，播放、切歌、音量等蓝牙命令被拒绝；`help`、
`debug`、`free`、`tasks`、`bt_power` 等系统命令仍可用。

## 实现边界

使用现有 LVGL tabview/switch/timer、ESP-IDF 公共 API 和 GMF 音频管线。
没有另写协议栈、显示驱动，也没有修改依赖子模块。

- UI/串口只提交请求，生命周期步骤在独立 `bt_power` task 执行。
- OFF 先禁止新业务操作，停止并复位音频读取任务。GMF v1.0 的蓝牙输入端口
  原本无限等待；项目通过 `esp_gmf_port_set_wait_ticks()` 设置 50 ms 对应的
  FreeRTOS ticks，避免停发音频时读取任务无法响应 STOP。
- 在同一个串行 esp_timer TASK 上放置晚于 GMF 200 ms 按键定时器的 300 ms
  回调，等待它返回后再销毁 AVRCP 上下文。这个边界依赖锁定的 GMF v1.0
  实现，升级 GMF、增加其他按键入口/持有时间时必须重新审查。
- 请求断开 A2DP，然后依次请求 AVRCP CT、TG、A2DP deinit，并等待真实
  Profile 完成事件。只收到 API 的入队成功，不能释放上下文。
- 完成 Host disable/deinit（退出 BTC 回调任务）后，才调用 GMF cleanup，最后
  disable Controller。GMF cleanup 内部重复调用已关闭的 SDK deinit，可能产生
  INVALID_STATE 日志；不能据此单独判定 OFF 失败，需结合最终状态与步骤。
- Controller 保持 INITED，不调用 controller deinit/mem_release。S31 的本机
  `esp_bt.h` 明确提示 controller deinit 尚未完整实现、不能保证再次初始化。
- ON 重新 enable Controller、初始化 Host/Profile、恢复原设备名、可发现性和
  AVRCP 通知，然后放行音频与业务命令。配置对象保持应用级生命周期。

回调观察通过链接器 `--wrap` 接入 SDK 注册/init API，先保留并转发 GMF 回调，
再记录完成信号；init 前先安装观察回调，避免异步 init 先于 GMF 注册回调。
UI ELF 的实际调用点已检查命中这些 wrapper；不是只验证测试替身。

参考：

- [ESP-IDF v6.1 Bluedroid 生命周期](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s31/api-reference/bluetooth/esp_bt_main.html)
- [ESP-IDF v6.1 AVRCP：先于 A2DP deinit](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s31/api-reference/bluetooth/esp_avrc.html)
- [ESP-IDF esp_timer：TASK 分发和阻塞停止](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s31/api-reference/system/esp_timer.html)

## 尚不能保证的恢复

这不是 Controller 硬复位，也不能保证恢复所有协议栈故障。

- Profile/disconnect/按键边界等待超时会进入 ERROR，保留仍可能被回调使用的
  上下文；可重试 OFF，不强行释放或进入下一代 ON。
- GMF task stop 和部分 Bluedroid API 自身可能无限等待；此时工作任务仍 busy，
  不通过删任务或强制 free 假装完成。屏幕继续显示步骤，但可能仍需串口
  `restart` 或硬件重启。
- GMF 内部初始化包含致命检查和自行清理的失败路径，本次没有修改它们；
  无法承诺内存耗尽、内部初始化异常也都能无重启恢复。
- 正常停止回调同步等待管线停止，以避免 GMF 立即释放流时还有读者使用它；
  如果停止返回不可恢复错误，安全检查仍可能重启。无卡/SD 写入不参与该等待。

## 上板验收（待完成）

先编译；串口/烧录由你确认：

```bash
cd /home/yiyuan/share/esp32
bash tests/validate_demo.sh
DEMO_UI=1 bash scripts/build_demo.sh
DEMO_UI=1 bash scripts/flash_demo.sh /dev/ttyUSB0
```

将最后一行端口替换为实际端口。本次开发没有执行烧录。

1. 无手机连接：UI 和串口各做 OFF/ON，观察最终状态与可发现性。
2. 荣耀手机连接但暂停：OFF 应完成，随后 ON 可重新连接。重点检查没有卡在
   `quiesce`，空音频输入应能退出。
3. 实际播放 44.1/48 kHz SBC：播放中 OFF，ON 后重新连接、播放，确认真实声音、
   暂停、切歌、音量、歌曲信息和封面逻辑；ON/PLAYING 日志不能证明物理出声。
4. 点击播放/下一首后立即 OFF，重复快速点开关。busy 应拒绝新请求，不能重启、
   触摸卡死或发生 use-after-free；界面不能提前声称 OFF/ON 成功。
5. 保持 Debug 模块开关开启，OFF 后页面/串口仍可控制；SD 无卡继续串口输出。
6. 连续 100 轮 ON→连接→播放→OFF，并用 `free` 记录每轮稳定后的空闲堆趋势。
   一次停止成功不证明无泄漏；比较同一稳定状态，不比较播放中和 OFF 两种状态。
7. 再复现原先 BT_OSI timer 错误，记录 `bt_power status`、失败 Step、原始日志及
   重开后的连接/实际播放结果，判定该故障是否能恢复。不要只测正常启停。

## 应用分区与 SD

本轮没有修改 `partitions_esp32s31.csv`。当前配置 Flash 为 16 MiB，factory 应用
从 0x10000 开始、大小 0x800000（8 MiB），结束于 0x810000，后面约 7.94 MiB
未分配。先确认实际硬件 Flash 容量与配置一致后，可以扩大应用分区：

- 例如改为 0xC00000（12 MiB），仍留约 3.94 MiB 给其他数据分区。
- 当前无 OTA；如果后续要双槽 OTA，需重新规划，不能把单槽最大空间都用掉。
- 改布局后要重新生成并烧录分区表与应用，不能只烧 app；先备份需要的数据，
  不默认 erase-flash 或删除 NVS。

SD 不能增加这块应用 Flash 分区，也不是当前固件的启动执行空间。可以用于现有
文本日志、封面缓存、音乐、外部字体等数据。字体从固件迁到 SD 后可间接减少 bin，
但需实现运行时加载和无卡回退；只插卡不会自动缩小已编入固件的字体。

依据：[ESP-IDF v6.1 分区表](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s31/api-guides/partition-tables.html)。

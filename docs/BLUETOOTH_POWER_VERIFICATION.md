# 蓝牙启停：本地验证记录

日期：2026-10-12。硬件启停、连接恢复、堆趋势和原故障恢复尚未验证。
用户先前确认的抗撕裂/日韩字体通过，不等于本次新功能已通过板端验证。

## 检查结果

- `bash tests/validate_demo.sh`：通过，两种配置源码选择、Debug 回归、启停顺序/
  适配器测试及有限蓝牙读等待的结构检查均通过。
- 真实 `bt_power.c` + SDK/RTOS 替身：10 轮模拟启停，各阶段错误、Profile 异步
  超时/迟到完成、关闭失败后 OFF 重试、注册前 init 回调、命令门禁和请求回滚通过。
  替身不验证线程调度、GMF 音频停止、射频或真实 SDK 初始化的失败行为。
- gcov：`bt_power.c` 196 行执行率 100%，分支 taken 81.11%；纯顺序模块 13 行
  执行率 100%，分支 taken 93.75%。不覆盖 main/console/stream_proc 的硬件集成，
  不宣称整个工程有 100% 覆盖率。
- 真实 LVGL 主机测试：22 个 UI 场景及真实 JPEG 解码回归通过，其中 5 个启停
  场景验证缺少后端时禁用、busy、实际 ON/OFF 状态、拒绝回滚和 ERROR 安全重试。
- UI 应用/JPEG 的 ASan/UBSan 构建同样通过；LVGL 依赖本身未全部加 sanitizer。
- 两种完整 ESP-IDF 构建通过。仅保留基线的
  `stream_proc_deinit_clk_sync_monitor` unused-function 警告，无新增构建错误。
- 实际 UI ELF 反汇编确认 GMF A2DP/AVRCP init 与 callback 注册调用进入六个
  `__wrap_*` 接口。
- 两个 bin 的 esptool `image-info` 校验和及 validation hash 均有效，目标 S31、
  Flash 16 MB / DIO / 80 MHz。产物在 GREEN 提交前生成，App version 为
  `bc54669-dirty`；提交后重新构建会更新版本及 bin hash。
- `git diff --check` 通过；未修改 SDK、GMF 子模块、分区表或默认配置。
- 已做本地人工调用链/生命周期审查。独立审查代理两次因服务额度/容量不可用，
  没有取得独立审查通过结论。

## 最终产物

应用分区仍为 0x800000（8 MiB）：

| 配置 | bin 大小 | 分区剩余 |
| --- | --- | --- |
| UI `build-ui/bt_audio.bin` | 0x6372c0 / 6,517,440 字节 | 0x1c8d40 / 22% |
| 无屏 `build/bt_audio.bin` | 0x2d8380 / 2,982,784 字节 | 0x527c80 / 64% |

当次 bin SHA256（源码路径在 `demo/a2dp_sink` 下）：

```text
UI:       bbdd97405b9dc3b1bc6a5fed94cf77534f2143d7df30e9b400dddffec8c69bd7
Headless: 394368d67ec8f011af4c969a6af0adca45cffdcb03067aec7001e3605e8397f1
```

两个生成的 partition-table.bin SHA256 一致：
`65e1a0f5c5b0e0039b71d9cfc26bb65836e64244bb979143886e730898bc5952`。

本机临时证据（/tmp 不是长期归档）：

```text
/tmp/esp32-bt-power-final-validation.log
/tmp/esp32-bt-power-ui-final-build.log
/tmp/esp32-bt-power-headless-final-build.log
/tmp/esp32-bt-power-ui-sanitizers.log
/tmp/esp32-bt-power-ui-final-tests.log
/tmp/esp32-ui-host.gGFOuw          # sanitizer UI/JPEG 测试产物
/tmp/esp32-power-host.g7MQ3t       # 启停替身/顺序测试及 gcov
```

此记录是编译/主机测试证据，不是上板验收报告。操作与板端测试步骤见
[蓝牙启停指南](BLUETOOTH_POWER_GUIDE.md)。

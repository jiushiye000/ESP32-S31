# ESP32-S31 蓝牙音频测试盒

当前工程将 ESP32-S31-Korvo-1 V1.1 配置为无屏经典蓝牙音箱：手机通过
A2DP/SBC 发送音乐，ESP-GMF 解码后将 PCM 送入板载 ES8389，AVRCP
Controller/Target 提供播放、暂停、切歌和音量控制。

工程从本机 `/home/yiyuan/share/esp32s31` 的 Demo 迁入。本仓库保存应用、
配置和脚本；ESP-IDF 与工具链继续使用本机独立安装。

## 固定基线

- 开发板配置：ESP32-S31-Korvo-1 V1.1，ES8389。
- ESP-IDF：本机 `v6.1`，位于 `/home/yiyuan/.espressif/v6.1/esp-idf`。
- ESP-GMF：`v1.0`，提交 `efc94f51c22d3af68a6f2f654f0eec9d13a46cb3`。
- 蓝牙角色：A2DP Sink、AVRCP Controller、AVRCP Target。
- 当前关闭：HFP、LE Audio、PBAP、LVGL UI。

应用基于乐鑫 Apache-2.0 许可的 `bt_audio` 示例，保留原始文件中的许可声明。
`third_party/esp-gmf` 是固定提交的 Git 子模块；组件通过相对 `override_path`
引用它。`demo/a2dp_sink/dependencies.lock` 记录其余组件的版本，
`main/idf_component.yml` 额外固定 `dl_fft` 为原 Demo 使用的 `0.7.0`，
避免首次生成板级配置时自动升级该依赖。

## 目录

```text
demo/a2dp_sink/          应用源码、默认配置、分区表与组件锁文件
scripts/idf_env.sh       激活本机 ESP-IDF 环境
scripts/build_demo.sh    生成板级代码并构建固件
scripts/flash_demo.sh    编译、烧录并打开串口监视器
tests/validate_demo.sh   检查工程结构和角色配置
third_party/esp-gmf/     固定版本的官方依赖子模块
```

## 校验与编译

新克隆本仓库后，先初始化子模块：

```bash
git submodule update --init --recursive
```

在本机目标目录执行：

```bash
cd /home/yiyuan/share/esp32
./tests/validate_demo.sh
./scripts/build_demo.sh
```

构建脚本自动激活 `/home/yiyuan/.espressif/tools/activate_idf_v6.1.sh`，
选择 `esp32s31` 和 `esp32_s31_korvo_1`，应用 `sdkconfig.defaults.korvo1`。
本机 IDF v6.1 将 S31 标为 preview 芯片，脚本已包含所需的 `--preview` 参数。

如使用其他安装位置，可通过 `IDF_ACTIVATE_SCRIPT` 指定支持 `-e` 输出环境变量
的激活脚本。迁移到其他目录或机器时，应重新生成构建缓存和 Board Manager 代码，
不要复制旧路径下的 `build/`、`managed_components/`、`sdkconfig` 或
`components/gen_bmgr_codes/`。

重新生成板级代码后编译：

```bash
REGENERATE_BOARD=1 ./scripts/build_demo.sh
```

主要输出：

```text
demo/a2dp_sink/build/bt_audio.bin
demo/a2dp_sink/build/bt_audio.elf
demo/a2dp_sink/build/bootloader/bootloader.bin
demo/a2dp_sink/build/partition_table/partition-table.bin
```

## 烧录与上板验证

确认实际开发板与上述 Korvo-1 配置一致后，在断电时接好扬声器。UART USB-C
连接电脑；扬声器供电按开发板说明使用专用电源 USB-C，打开电源开关。

指定实际串口后执行：

```bash
./scripts/flash_demo.sh /dev/ttyUSB0
```

只有一个 `/dev/ttyACM*` 或 `/dev/ttyUSB*` 时，脚本也支持不传端口。
按 `Ctrl-]` 退出串口监视器。

手机搜索 `esp_classic_XXXX` 并配对后，从较低音量开始播放本地音乐。
`XXXX` 为蓝牙 MAC 地址最后两个字节。串口常用命令：

```text
help
play
pause
next
prev
vol_set 30
disconnect
```

保留 Board Manager、ES8389、I2S、连接、SBC Codec 参数、Stream Start 和
`bt2codec` Pipeline 日志。Korvo-1 的 ES8389 使用 TDM，当前输出配置为
48 kHz；上板验证应确认声音速度、音量和播放/暂停行为。

构建成功仅确认固件可生成；配对、播放和物理出声需单独验证。
后续再增加首包、首帧 PCM、Codec 写入时间戳和串口自动化测试。

本次迁移的编译结果、版本和固件校验见 [编译验证记录](docs/BUILD_VERIFICATION.md)。

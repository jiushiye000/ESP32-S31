# 显示与 Media 改造（2026-10-12）

## 已实现与复用来源

- Debug：深色页面显式不透明，卡片/标题/状态显式浅色，避免 LVGL theme 的
  卡片前景覆盖父页面样式。开关后端和 SD 策略保持原逻辑。
- 抗撕裂：`board_overlays/rgb_antitear` 使用官方
  [Board Manager amend](https://docs.espressif.com/projects/esp-board-manager/en/latest/create-board/amend.html)
  将 LCD `num_fbs` 设为 2，不改官方引脚/时序/18MHz PCLK。
  `esp_lvgl_port` 使用全屏 direct mode，直接借驱动的两块 framebuffer，
  由官方 port 在 VSYNC 后同步；不是应用自行写 ISR。
  两块 800×480 RGB565 framebuffer 共 1,536,000 字节 PSRAM。
  初始不启用 bounce buffer，不混入其他芯片的 ISR workaround。
  参考[官方 RGB 示例](https://github.com/espressif/esp-bsp/blob/master/components/esp_lvgl_port/examples/rgb_lcd/main/main.c)。
- 封面：屏幕 defaults 开启 `BT_AVRCP_CT_COVER_ART_ENABLED`，沿用 GMF 已有
  linked thumbnail / GOEP / OBEX / JPEG 链路；未增加新 BIP/蓝牙协议框架。
  参考[官方 AVRCP API](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s31/api-reference/bluetooth/esp_avrc.html)。
  收到真实 JPEG 才替换占位，未收到时显示 `Cover not received`。
- 字体：原 SC 汉字库 + Noto Kana/Hangul 静态 fallback；覆盖全部现代韩文音节，
  无卡也能显示。来源、OFL 许可证和转换命令见 `main/bt_ui/fonts/README.md`。
  metadata 在 256 字节内按完整有效 UTF-8 码点复制，title/artist 分开更新，
  长歌名用 LVGL flex 避免与歌手重叠，标题/歌手有高度预算，超长文本用官方
  LONG_DOT 显示省略号；不是无限长标题显示或全 Unicode/emoji 字库。
- 播放：命令返回 ESP_OK 时立即切换播放/暂停图标，失败恢复；手机的后续
  PLAY_STATUS 通知仍校正 UI。命令已提交不等于手机已播放或已经物理出声。

## 封面边界（必须保留）

UI 对编码输入限制 512KiB、解码输出不超过 340×340 RGB565，队列最多两项。
JPEG 解码在 worker、LVGL 锁外完成，只有 widget/descriptor 安装持锁；清除时
先隐藏并清理缓存，再释放像素。调用者保留编码数据所有权。
进入官方 decoder 前验证 SOI、全部至 SOS 的段边界、唯一 SOF0、组件长度、
尺寸及 baseline 格式，避免官方 wrapper 的无界 skip 路径。

切歌与断线清除旧标签/封面；app generation 防止**已排队或正在解码**的旧图安装。
但 GMF v1.0 完成事件只含 format/size/data，不含 track UID、image handle 或请求代次。
旧图若在切歌后才从蓝牙层交付，app 不能判断归属。官方 GMF 在传输中会忽略新
handle，错误/断开路径也存在 transmitting 未复位的限制。本轮不通过每曲断开
Cover Art 来伪造可靠取消，也没有修改 SDK/GMF/managed dependencies。
切歌清除后，若手机仍用同一 image handle，GMF 也可能不再交付 JPEG，界面会
保持占位；“同封面不重复下载”与应用清除之间仍需后续在 GMF 身份/缓存层协调。

此外，UI 512KiB 上限发生在完整 JPEG 已由 GMF 接收之后，**不是端到端下载内存上限**。
连续快速切歌、下载失败后的恢复和手机兼容性必须实测，不能声称所有封面与最新歌一致。
荣耀 WinRT 为本轮用户指定的验证手机；尚未验证它和具体播放器是否实际提供 Cover Art。

## 编译和现有配置迁移

从 `/home/yiyuan/share/esp32` 执行：

```bash
DEMO_UI=1 ./scripts/build_demo.sh
```

脚本会因 amend 更新或旧 `num_fbs=1` 自动重新生成 Board Manager 代码，明确传入
`bmgr -b esp32_s31_korvo_1 -a board_overlays/rgb_antitear`。新建 sdkconfig.ui 自动
读取新 defaults；**已有 sdkconfig.ui 中的关闭值优先于 defaults**，须先备份，再
通过屏幕配置的 menuconfig 开启 Cover Art。脚本会检查实际值，不接受“编译成功但功能未开”。

本机已有配置已单项开启，原 UI/无屏 sdkconfig 备份在
`/tmp/esp32-ui-config-20261012.vovXA5`（临时目录可能被系统清理）。不需删除 NVS 或 erase-flash。
其他机器修改现有配置可用：

```bash
source scripts/idf_env.sh
cd demo/a2dp_sink
"${idf_python}" "${IDF_PATH}/tools/idf.py" --preview -B build-ui \
  -D "SDKCONFIG=$PWD/sdkconfig.ui" menuconfig
```

选择 AVRCP controller Cover Art support 后保存。生成配置需有
`CONFIG_BT_AVRCP_CT_COVER_ART_ENABLED=y`、`CONFIG_BT_GOEPC_ENABLED=y`，LCD 生成代码需有
`.num_fbs = 2`。启动时应看到：

```text
BT_UI: RGB LCD: framebuffers=2, avoid_tearing=1, direct_mode=1
```

## 烧录与板端验收

确认旧串口 monitor 已退出，再从仓库根目录执行：

```bash
DEMO_UI=1 ./scripts/flash_demo.sh /dev/ttyACM0
```

端口按本机实际设备替换。本轮没有自动烧录，也没有远端写操作。

1. 无卡启动，Debug 标题、SD 说明、卡片模块名和底部状态都可读，开关仍有效。
2. 看启动 framebuffer/avoid_tearing 日志，然后持续滚动 Debug、切换 Media 并播放
   至少 10 分钟。确认撕裂、黑屏/错位、触摸及音频共存；日志不能替代肉眼结果。
3. 荣耀 WinRT 播放有真实专辑封面的歌曲，记录具体音乐 App。手机若缓存旧 AVRCP
   SDP，可在双方忘记配对后重新配对；这是按需诊断，不需要 erase-flash。
4. 查看 `CT cover art state evt: cover_art_state 1` / `CT cover art final data event`、应用
   `Cover art: size ... format ...`；打开 AVRC/APP debug 后看解码诊断。
   未出现 final JPEG 不等于 UI 故障，先确认手机/播放器服务实际提供封面。
5. 分别测普通切歌、同专辑/同封面、快速连续切歌、无封面、断线重连及下载失败恢复，
   核对显示图与歌名，不只检查“出现图片”。串图/卡住请保留 AVRC/APP/OSI 日志。
6. 歌名/歌手含 `ひらがな・カタカナ`、`한국어 한글`、中英混排与长文本。
7. 点播放见暂停图标，点暂停见播放图标；在手机上切换播放/暂停应反向同步。
   未连接时命令拒绝应恢复按钮，不能仅变色或假装成功。

本机编译/原生回归的详细证据见 [验证记录](MEDIA_DISPLAY_VERIFICATION.md)。

# UI 维护入口

这里继续使用现有 Espressif `bt_ui`、Board Manager、`esp_lvgl_port`、LVGL 和
`esp_jpeg`，不维护另一套显示、触摸或 JPEG 框架。
这是页面拆分阶段，仍属于原 `main` 组件；不宣称已完成独立 UI 组件化。

| 文件 | 职责 / 修改入口 |
| --- | --- |
| `bt_ui.h` | 应用使用的公共 API 和注入的操作回调，当前契约不变 |
| `bt_ui.c` | 页面组装、字体初始化、应用更新接口、共享状态刷新计时器 |
| `ui_media.c` | 标题/歌手、播放暂停、前后曲、CIS/BIS 图示与媒体布局 |
| `ui_cover.c` | 有界封面队列、JPEG 校验/解码、generation 校验、像素与图像缓存所有权 |
| `ui_debug.c` | 模块/SD 开关、Debug 快照及后端状态呈现，不在屏幕绘制日志正文 |
| `ui_power.c` | System 页与 Power 操作回调，只呈现后端真实状态 |
| `ui_dialer.c` | 既有拨号页、号码与通话按钮；页面拆分不等于新增 HFP 能力 |
| `ui_display.c` | 官方显示/触摸初始化，以及已验证的 RGB 抗撕裂参数 |
| `ui_volume.c` | 浮动音量条与隐藏计时器 |
| `bt_ui_internal.h` | 页面之间的私有状态/接口，不供应用包含 |
| `sources.cmake` | UI 源文件清单，固件和实际 LVGL 主机测试共同使用 |

## 维护规则

- 蓝牙操作继续由应用注入回调；不要在页面直接创建/销毁协议栈或音频管线。
- Debug 状态使用 `bt_debug` 组件，不在 UI 中复制持久化、日志等级或 SD 文件逻辑。
- 共享 500 ms 计时器由 `bt_ui.c` 创建，依旧先刷新 Power、再刷新 Debug。
  页面不互相创建或依赖对方的计时器。
- 封面消息持有编码数据的副本；JPEG 解码不持有 LVGL 锁。安装像素时先丢弃
  LVGL 图像缓存，再释放旧像素；generation 校验防止清封面后重新出现旧图。
- 不把借用的 Bluetooth stream handle/codec 指针放入队列；当前快照仍为值语义。
- 当前仍为单实例、固件生命周期 UI；此次没有新增销毁/热重建 API。
  不把拆成多个文件理解为已支持多实例。
- 后续应用呈现适配层与 Power 运行时接口明确后，再提取独立 UI 组件，避免反向依赖 `main`。

## 验证

在仓库根目录运行：

```sh
bash tests/validate_demo.sh
bash tests/test_ui.sh
UI_HOST_SANITIZERS=ON bash tests/test_ui.sh
bash scripts/build_demo.sh
DEMO_UI=1 bash scripts/build_demo.sh
```

`check_ui_pages.cmake` 检查 UI OFF/ON 与 Classic/LE 的源文件选择；它不是 LE 固件
编译或板端验证。实际验收记录见仓库 `docs/REFACTOR_VERIFICATION.md`。

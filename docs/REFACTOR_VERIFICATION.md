# 解耦验证记录

## 重构前基线（2026-10-12）

源码起点：`a4fe8bb`，ESP-GMF `v1.0`，ESP-IDF `v6.1`。
本轮重新执行，不沿用旧构建产物作为通过证据。

| 检查 | 命令 | 结果 |
| --- | --- | --- |
| 结构、配置、Debug、Power 主机测试 | `bash tests/validate_demo.sh` | PASS |
| 实际 LVGL 与 JPEG 主机测试 | `bash tests/test_ui.sh` | 22 项 UI 场景与实际 JPEG 回归 PASS |
| 无屏固件 | `bash scripts/build_demo.sh` | PASS，应用大小 `0x2d8380` |
| UI 固件 | `DEMO_UI=1 bash scripts/build_demo.sh` | PASS，应用大小 `0x6372c0` |

两种固件的应用分区保持 `0x800000`。既有 CMake 最低版本提示是基线警告，
没有因此改动 CMake 版本或 SDK。

Debug policy/storage 主机行覆盖率 97.30%；Power adapter 主机 mock 行覆盖率 100%。
这些数字不包含真实 RTOS/控制器/SD 集成，不代表板端成功。

用户此前确认 RGB 撕裂和日韩显示已解决。本轮没有连接或烧录开发板；
蓝牙运行时启停及本轮结构迁移后的硬件回归仍需独立执行。

## 实施状态

- 阶段 1：来源、维护约定和重新执行的基线已记录。
- 阶段 2：Debug 组件与独立串口命令，本地主机/构建验证完成；用户确认此版板端功能无新增问题。
- 阶段 3–6：待前一阶段验收后推进，不宣称已经完成。

## 阶段 2：Debug 组件（2026-10-12）

RED 提交 `eb227e5`：`bash tests/test_debug_component.sh` 实际运行失败，
原因是 `main` 仍注册 `bt_debug.c`，并非测试语法或依赖安装错误。
GREEN 将原实现迁入 `components/bt_debug/src/`，公共头移入 `include/`，
从 `cmd_reg.c` 抽出 CLI，并通过官方 `EXTRA_COMPONENT_DIRS` 接入应用。

| 检查 | 结果 |
| --- | --- |
| 新组件边界（UI OFF/ON）与真实 CLI 主机测试 | PASS；CLI 行覆盖 100%，分支 taken 94.44% |
| `bash tests/validate_demo.sh` | PASS；含既有 Debug policy/storage 和 Power 回归 |
| `bash tests/test_ui.sh` | 22 项 UI 场景及实际 JPEG 回归 PASS |
| `UI_HOST_SANITIZERS=ON bash tests/test_ui.sh` | 同一套 UI/JPEG ASan、UBSan 回归 PASS |
| 真实 CLI 单独使用 ASan、UBSan | PASS |
| `bash scripts/build_demo.sh` | PASS，应用大小 `0x2d83e0` |
| `DEMO_UI=1 bash scripts/build_demo.sh` | PASS，应用大小 `0x637330` |
| Shell 语法与 `git diff --check` | PASS |
| `interrogate` 独立只读交叉审查 | 三份审查均无 actionable findings；非板端验证 |

已核对两个 IDF 构建中的 `libbt_debug.a` 为独立组件，包含后端、policy、storage、
console 四个对象；没有依赖 main、LVGL 或音频应用组件。无屏 ELF 的
`lv_ / lvgl_port_ / bt_ui_` 定义符号为 0，UI ELF 为 1144。
当前阶段没有清理原应用 manifest 的 UI 依赖声明，不把这一结果描述成依赖解析开销已消除。

### 等价和保护证据

六个原文件的 Git blob 在迁移前后逐一相同；`debug_config()` 函数内容也与基线相同。
只改变组件注册、头文件搜索路径和 Debug 命令的注册归属。
`main.c`、UI 实现、`stream_proc.c`、Power 状态机/命令保护、Kconfig 和第三方代码未改。

迁移前后配置与分区/锁文件 SHA-256 相同：

| 文件 | SHA-256 |
| --- | --- |
| `sdkconfig` | `27c49d766921f5f11d76dfaed02b9ecf1289307c49f2851c4e982b04e9486a64` |
| `sdkconfig.ui` | `f40aaf41d06ed005219b8e4335c5063a6132303f80d38e16bd63ba3e783a627a` |
| `dependencies.lock` | `42deb9025e5ae2eaaf615648cefa4648defbdbb784e696fafd5ebb060d5197d8` |
| `partitions.csv` | `3656429679628e3ac554a1bca81f16dbd7c18253c859b62312188c6b23ea1811` |

构建保留现有 SDK/Board Manager 的 include/source ownership 提示、上游 Kconfig bool
默认值提示，以及原 `stream_proc_deinit_clk_sync_monitor` 未使用警告。
它们不涉及新 Debug 组件；本阶段未改第三方代码来消除这些提示。

### 板端交接门槛

先回归本阶段，再继续调整 UI、音频与电源边界，避免多个未上板验证的结构变化叠加。
仍使用现有烧录脚本，屏幕版设置 `DEMO_UI=1`，不擦除 NVS、不改变分区。

1. 手机连接、播放/暂停、切歌、音量、断开重连正常；屏幕、触摸、日韩标题无回退。
2. `help` 有 `debug`；CLI 的 `debug A2D on/off` 与屏幕开关相互同步，
   AVRC 等其他模块不受影响。保存完成后重启，原有设置仍能恢复。
3. 无卡执行 `debug sd on` 为 NO CARD / serial only，音频/串口继续工作；
   `debug all off` 不关闭 SD 请求，完成后 `debug sd off`。
4. 蓝牙 `bt_power off/on/status` 和屏幕开关保持既有行为；正在播放时 OFF、再 ON 后
   可重新连接。此项本轮未板端验证，不能以主机 mock 代替。

SD 实际文件保存仍要求插卡单独验证；这一阶段不新增热插拔或文件传输功能。

### 用户验收（2026-10-12）

用户确认当前版本“功能没有其他问题”，允许继续下一阶段。验收基线为
`34c1f17`（Debug 组件 GREEN）。本记录不把概括性确认扩写成逐项测试数据，
也不据此宣称已完成插卡写入、压力测试或显示时序测量。
下一阶段先拆 UI 页面，不同时改动蓝牙事件分发和 Power 生命周期。

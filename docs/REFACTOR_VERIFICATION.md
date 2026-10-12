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
- 阶段 2：Debug 组件与独立串口命令，待实施与验证。
- 阶段 3–6：待前一阶段验收后推进，不宣称已经完成。

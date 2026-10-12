# 渐进解耦与维护约定

目标是让现有蓝牙测试盒容易维护，不增加角色、不升级 SDK、不重写官方框架。
工作分支为本地 `refactor/component-boundaries`，起点为 `a4fe8bb`。
不推送、不改远程仓库、不自动烧录。

## 阶段与边界

1. 固定源码来源、构建与测试基线。
2. 抽取已有 Debug 后端及其串口命令；保留模块索引、NVS 和 SD 行为。
3. 拆分 UI 页面和应用呈现适配层；保留官方 LVGL、Board Manager 和图像所有权。
4. 明确音频运行时、命令保护、Power 生命周期接口，再封装 Bluedroid adapter。
5. 有实际多消费者需求时才加入薄事件路由，不预建通用发布订阅框架。
6. 先使用单应用和独立构建配置；确有启动、分区或部署差异时再拆薄应用入口。

每个阶段单独提交和验证。移动文件与行为修复分开；不混入 SDK 升级、分区变更、
HFP/Source/LE 功能实现。完成状态和实际验证见 [解耦验证记录](REFACTOR_VERIFICATION.md)。

## 不可破坏的契约

- UI、CLI 使用同一个 Debug 后端，不分别维护模块开关状态。
- registry 只追加模块；现有 NVS `bt_debug_v1` 和 bit 编号不变。
- SD 由 Board Manager 挂载；日志 hook 不做文件 IO、不等待队列、不递归打印。
- Power 关闭期间，UI/CLI/自动操作必须遵守相同的命令保护。
- GMF 的音频停止和释放回调保留同步确认；不能把借用的 stream handle 放入异步队列。
- metadata/封面跨回调传递时需要拥有数据；只复制结构中的指针不够。
- 当前 GMF v1.0 没有向应用转发 Profile init/deinit completion；不能用断连事件
  替换 Power 对这些完成事件的等待。现有 `--wrap` 在获得等价正式接口前保留。
- 自有组件不得反向依赖 `main`。需要应用操作时定义明确接口或注入回调。
- 未支持的角色显示能力不可用；只有产品明确要求该能力时才把缺失作为构建错误。

## 上游来源

ESP-IDF 使用本机 `v6.1`，ESP-GMF 子模块固定在
`efc94f51c22d3af68a6f2f654f0eec9d13a46cb3`（`v1.0`）。
示例基目录为 `packages/esp_bt_audio/examples/bt_audio/main/`。
下面记录起点对应的 Git blob，供移动文件后的三方比较使用；它们不是本地改后文件的 hash。

| 本地派生文件 | 上游相对文件 | 上游 blob |
| --- | --- | --- |
| `demo/a2dp_sink/main/main.c` | `main.c` | `31ec08787315782797fee0345b73ea092ccf411b` |
| `demo/a2dp_sink/main/cmd_reg.c` | `cmd_reg.c` | `5f8db0ed9510f30e636fbdd9c55cce53ed20b0f9` |
| `demo/a2dp_sink/main/stream_proc.c` | `stream_proc.c` | `e5e67345fdd164cbe87afda6c9d136c3354b18fb` |
| `demo/a2dp_sink/main/pool_reg.c` | `pool_reg.c` | `029b52b454f35810c278244301b22f03fda372f8` |
| `demo/a2dp_sink/main/Kconfig.projbuild` | `Kconfig.projbuild` | `109a39166172bd135e76c568656427a5d1607734` |
| `demo/a2dp_sink/main/bt_ui/bt_ui.c` | `bt_ui/bt_ui.c` | `70378251467fd6a3e90cd1d98954b203e816e86f` |

`bt_debug*` 和 `bt_power*` 是项目自有模块，不是上游派生文件。
移动上游派生文件时同步更新此表；不能为了减少 diff 丢掉已验证的安全修复。

升级时先比较旧、新上游的目标文件，再以旧上游文件为 base 做三方合并。
不要直接编辑 `third_party` 或 `managed_components`；确需修改依赖时使用记录版本、
应用顺序和验证方式的显式 patch。升级与结构重构必须分开验证。

## 配置和验证

- 保留无屏 `build/ + sdkconfig` 和 UI `build-ui/ + sdkconfig.ui`。
- defaults 只提供默认值，不保证覆盖已有 sdkconfig。先保存/导出本地修改、备份并
  对比，再使用独立配置和构建目录做干净重生成；不直接删除用户配置。
- 保留 Board Manager 生成的 defaults 和 RGB 抗撕裂 amend 的现有加载顺序。
- 使用组件的 `REQUIRES` / `PRIV_REQUIRES` 表达公开/私有依赖，不靠 main 的特殊待遇
  让自有组件隐式获得全部头文件。
- 执行已有主机测试、真实 UI/JPEG 测试及两种 IDF 构建；新增角色只有实际构建通过后
  才加入“已验证”矩阵。结构检查不是固件构建，主机 mock 不是板端验证。
- 修改启停、音频、显示生命周期后，上板检查配对、播放/暂停、切歌、音量、断连重连、
  蓝牙 OFF/ON。有卡保存与无卡退化分别验收。
- 行数用于发现职责过多，不作为强制拆文件指标。测试覆盖报告明确排除无法在主机
  执行的 IDF/板级集成，不能把纯逻辑覆盖率当作整机覆盖率。

官方构建机制依据：[ESP-IDF v6.1 组件与 EXTRA_COMPONENT_DIRS](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s31/api-guides/build-system.html)。

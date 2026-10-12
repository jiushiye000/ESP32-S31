# bt_debug

项目级 Debug 组件。UI 与 CLI 共用它；不依赖 LVGL、GMF、Power 或应用 `main`。

- `include/bt_debug.h`：模块开关、能力、保存状态和 SD 请求的既有公共 API。
- `include/bt_debug_console.h`：在 REPL 启动前注册 `debug` 命令，向调用者返回错误。
- `src/bt_debug_policy.c`：纯逻辑 registry；新模块只能追加，NVS bit 编号不可重排。
- `src/bt_debug_storage.c`：可在主机执行的有界文件存储，保留排他创建与 fsync。
- `src/bt_debug.c`：官方 ESP 日志、NVS、RTOS、Board Manager 适配及异步 writer。

不自行挂载 SD，不改变 Profile 角色，不把日志文件 IO 放进蓝牙回调。
没有新增配置开关：两种既有固件都保留 Debug 后端，运行时默认和持久化语义不变。
UI/HFP/BLE 的可用性仍按原来的全局 sdkconfig 符号判断。

从仓库根目录执行：

```bash
bash tests/test_debug.sh
bash tests/test_debug_component.sh
```

主机测试验证 policy/storage 和真实 CLI 的参数/状态转发，不代替 ESP/NVS/SD 板端验证。

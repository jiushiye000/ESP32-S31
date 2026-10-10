# 工程迁移编译验证

验证日期：2026-10-10（Asia/Shanghai）。

## 范围与版本

- 目标仓库：`/home/yiyuan/share/esp32`，分支 `main`。
- 工程：`demo/a2dp_sink`；目标芯片 `esp32s31`。
- 开发板配置：`esp32_s31_korvo_1`，ES8389。
- ESP-IDF：`v6.1`，提交 `fff9895c`。
- ESP-GMF 子模块：`v1.0`，提交 `efc94f51c22d3af68a6f2f654f0eec9d13a46cb3`。
- 原 Demo 的应用 C/H 文件及构建、烧录、检查脚本保持原样。
- `dl_fft` 在 manifest 中固定为 `0.7.0`，与原 Demo 的锁文件一致。
- 最终锁文件中的组件版本和哈希与原 Demo 一致；新增直接依赖项、manifest
  哈希及 registry URL 尾部斜杠由组件管理器更新。

## 实际执行

```bash
cd /home/yiyuan/share/esp32
bash tests/validate_demo.sh
bash -n scripts/idf_env.sh scripts/build_demo.sh scripts/flash_demo.sh tests/validate_demo.sh
bash scripts/build_demo.sh
git diff --check
git diff --cached --check
```

板级配置在目标路径重新生成。首次编译后，因发现 `dl_fft` 自动升版，添加了
原版本约束，再从空 `build/` 目录完成最终全量编译；退出码为 0，完成
2706 个构建步骤，并输出 `Project build complete`。

构建日志：`demo/a2dp_sink/build/log/idf_py_stdout_output_2885196`。

## 结果

- 结构检查、Shell 语法检查及 Git 差异空白检查通过。
- 新构建缓存、编译命令、生成的 Board Manager 代码和组件 manifest 未发现
  `/home/yiyuan/share/esp32s31` 的旧路径引用。
- ELF、应用固件、Bootloader 和分区表均已生成。
- 应用固件大小：`2808208` 字节（`0x2ad990`）。
- 应用分区大小：`0x800000`；剩余空间约 67%。
- `esptool image-info` 检测为 ESP32-S31，Checksum 与 Validation hash 均有效。
- 固件记录的编译时间：`Oct 10 2026 14:51:31`。
- Flash 参数：16 MB、80 MHz、DIO；实际烧录参数以生成的 `flasher_args.json` 为准。
- 构建产生两个既有的未使用函数 warning；另有上游 CMake/Kconfig 提示，未阻断构建。

`bt_audio.bin` 文件 SHA-256：

```text
50285b8def56aeca550d9eea860cd3ddd683a9079d02156f28ad42fae61db7d3
```

`bt_audio.elf` 文件 SHA-256：

```text
4e6990465fc692daf54041fdcbb116a19e61d0be3df91d97ca25953ca3202d1f
```

本次验证覆盖本机编译与固件格式。尚未烧录或验证配对、音量及物理出声。
原 `/home/yiyuan/share/esp32s31` 目录保留；迁移更改仅在本地，未提交或推送。

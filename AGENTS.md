# Project Agent Instructions

## Build and Target

- `esp_env.sh` is a machine-local environment helper. Create or configure it locally and do not commit it to the remote repository; it is already excluded by `.gitignore`.
- Activate the project environment from Bash with `source ./esp_env.sh`, then build with `idf.py build`.
- Flash and monitor with `idf.py -p <PORT> flash monitor`, using the port connected to the board. Exit Monitor with `Ctrl+]` before rebuilding or flashing.
- Target configuration is inconsistent between `esp_env.sh` and the current `sdkconfig`/build output. Confirm the actual chip and ESP-IDF target support before changing it; do not run `set-target` based only on the board name.

## INA Component

- The public API is `components/ina/ina_driver.h`; model implementations are under `components/ina/src/`.
- `main/main.c` currently uses the INA169 ADC API. INA219 and INA226 have separate APIs and drivers; their I2C measurement results share `ina_i2c_measurement_t`.
- Keep model-specific register settings and measurement conversion in the corresponding driver, not in the application. Consult [doc/INA219_HANDOFF.md](doc/INA219_HANDOFF.md) for existing wiring and measurement notes, and verify them against current source and hardware before relying on them.

## Hardware Checks

- Do not infer physical wiring or resistor values from a module name. The drivers assume a `100 mOhm` shunt; INA169 also assumes a `10 kOhm` output load. Verify markings or the module schematic before changing conversion constants.
- I2C pins and addresses are model-specific constants. The drivers currently use SDA GPIO1/SCL GPIO0, while the handoff wiring table records GPIO42/GPIO38. Confirm the board wiring before using or changing an I2C driver.
- INA169 reports an analog output voltage, not the shunt voltage directly. Its current estimate requires valid ADC voltage calibration and a zero-current offset calibration.

## Code Style

- Format C/C++ files according to the root `.clang-format`: Allman braces, 4-space indentation without tabs, no automatic line wrapping, left-aligned pointers, and preserve include order.

## Tool Use

- Do not access the Espressif documentation MCP server unless the user explicitly asks for it.
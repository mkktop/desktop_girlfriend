# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

Claude Code runs in a MSys/Git Bash shell, which ESP-IDF's `idf.py` rejects (detects `MSYSTEM` env var and skips `main()`). To compile, use the provided `build.bat` which runs via CMD with the correct environment:

```bash
# Claude Code 编译（通过 build.bat 在 CMD 环境下绕过 MSys 检测）
cmd.exe //C "D:\work\desktop_girlfriend\build.bat build" 2>&1 | tail -50

# 清理后编译
rm -rf build && cmd.exe //C "D:\work\desktop_girlfriend\build.bat build" 2>&1 | tail -50
```

For users in an ESP-IDF terminal (CMD/PowerShell), use standard commands:

```bash
idf.py build                          # Build the project
idf.py -p <PORT> flash                # Flash to device
idf.py -p <PORT> monitor              # Serial monitor
idf.py -p <PORT> flash monitor        # Build, flash, and monitor
idf.py menuconfig                     # Configure SDK options
idf.py fullclean                      # Clean all build artifacts
```

`build.bat` sets `MSYSTEM=` and adds cmake/ninja/toolchain to PATH, then calls `idf.py` via the ESP-IDF Python venv.

## Architecture Overview

This is an ESP32-S3 embedded GUI project using LVGL for a "desktop girlfriend" device.

### Technology Stack
- **Framework**: ESP-IDF v5.4.1
- **MCU**: ESP32-S3 (dual-core)
- **Graphics**: LVGL v9.5.0 (managed component)
- **Display**: ST7789 (240×320, SPI @ 80MHz)
- **RTOS**: FreeRTOS

### Board Abstraction

项目通过**板卡抽象层**支持多个硬件板卡，编译时通过 Kconfig 选择：

| Path | Purpose |
|------|---------|
| `main/boards/board.h` | 板卡配置结构体（`board_t`）+ 单例接口（`board_get_instance()`） |
| `main/boards/<board_name>/board.c` | 各板卡的具体引脚和参数配置 |
| `main/boards/common/` | 板卡共享驱动（背光、按钮等） |
| `main/Kconfig.projbuild` | 板卡选择菜单（`menuconfig` 中可见） |

`board_t` 通过嵌套结构体分组外设配置：`lcd_cfg_t lcd`、`wifi_ap_cfg_t wifi_ap`，扩展时只需添加新的 `xxx_cfg_t`。

添加新板卡只需：
1. 创建 `main/boards/<新板卡名>/board.c`，填充 `board_t` 结构体
2. 在 `Kconfig.projbuild` 添加选项
3. 在 `main/CMakeLists.txt` 添加 `elseif` 分支

### Module Structure

| Path | Purpose |
|------|---------|
| `main/main.c` | Application entry point - initializes NVS, event, WiFi, display, UI |
| `main/modules/event/app_event.c` | Lightweight event callback system for inter-module communication |
| `main/modules/display/app_display.c` | Display hardware init (esp_lcd + esp_lvgl_port, Core 1, priority 1) |
| `main/modules/display/ui/ui_manager.c` | Page manager - handles page switching and event-driven UI updates |
| `main/modules/display/ui/ui_home.c` | Home page UI |
| `main/modules/wifi/app_wifi.c` | WiFi provisioning (AP+STA mode) with embedded HTTP server |
| `main/resources/html/index.html` | WiFi provisioning web page |

### Display Stack

- **LCD Driver**: ESP-IDF built-in `esp_lcd_new_panel_st7789()` via `esp_lcd` framework
- **LVGL Adapter**: `esp_lvgl_port` component (managed component v2.7.x)
- **SPI**: `SPI2_HOST` @ 80MHz, DMA auto, polling mode
- **Buffer**: Partial render, `width * 20` pixels, single buffer, DMA capable
- **Thread Safety**: `lvgl_port_lock()` / `lvgl_port_unlock()` for cross-task LVGL access

### Hardware Pins (LCD)

All pin definitions are in `main/boards/desktop_girlfriend_V1/board.c` (current board).

### WiFi Provisioning

AP config (SSID, password, channel) is defined in `main/boards/desktop_girlfriend_V1/board.c`. HTTP endpoints:
- `/` - Configuration page
- `/scan` - Scan for WiFi networks
- `/connect` - Connect to WiFi
- `/disconnect` - Disconnect WiFi
- `/status` - Get connection status

## Code Conventions

Use Doxygen-style comments in Chinese:

```c
/**
 * @file filename.c
 * @brief 功能简述
 * @author mkk
 * @date YYYY-MM-DD
 * @note 额外说明
 */

/**
 * @brief 函数功能简述
 * @param param1 参数说明
 * @return 返回值说明
 */
```

- Use `/* */` for inline comments, not `//`
- Chinese comments use full-width punctuation (，。：；)

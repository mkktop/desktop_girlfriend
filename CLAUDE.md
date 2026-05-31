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
idf.py -p <PORT> flash                # Flash to device (includes assets partition)
idf.py -p <PORT> monitor              # Serial monitor
idf.py -p <PORT> flash monitor        # Build, flash, and monitor
idf.py menuconfig                     # Configure SDK options (board selection, fonts, etc.)
idf.py fullclean                      # Clean all build artifacts
```

`build.bat` sets `MSYSTEM=` and adds cmake/ninja/toolchain to PATH, then calls `idf.py` via the ESP-IDF Python venv.

## Architecture Overview

This is an ESP32-S3 embedded GUI project using LVGL for a "desktop girlfriend" device.

### Technology Stack
- **Framework**: ESP-IDF v5.4.4
- **MCU**: ESP32-S3 (dual-core)
- **Graphics**: LVGL v9.5.0 (managed component)
- **Display**: ST7789 (240×320, SPI @ 80MHz)
- **RTOS**: FreeRTOS
- **Fonts**: xiaozhi-fonts ~1.6.0 (Alibaba PuHui + CBin loader + FontAwesome icons)
- **Assets**: esp_mmap_assets ~1.4.0 (partition packaging + mmap zero-copy)

### Board Abstraction

项目通过**板卡抽象层**支持多个硬件板卡，编译时通过 Kconfig 选择：

| Path | Purpose |
|------|---------|
| `main/boards/board.h` | 板卡配置结构体（`board_t`）+ 单例接口（`board_get_instance()`） |
| `main/boards/<board_name>/board.c` | 各板卡的具体引脚和参数配置 |
| `main/boards/common/` | 板卡共享驱动（背光、按钮等） |
| `main/Kconfig.projbuild` | 板卡选择菜单（`menuconfig` 中可见） |

`board_t` 通过嵌套结构体分组外设配置：`lcd_cfg_t lcd`、`wifi_ap_cfg_t wifi_ap`、`font_cfg_t font`，扩展时只需添加新的 `xxx_cfg_t`。

添加新板卡只需：
1. 创建 `main/boards/<新板卡名>/board.c`，填充 `board_t` 结构体
2. 在 `Kconfig.projbuild` 添加选项
3. 在 `main/CMakeLists.txt` 添加 `elseif` 分支

### Module Structure

| Path | Purpose |
|------|---------|
| `main/main.c` | Application entry point - initializes modules, runs EventGroup-driven main loop at priority 10 |
| `main/modules/event/app_event.c` | Event system: FreeRTOS EventGroup + observer pattern + Schedule (deferred callbacks) |
| `main/modules/display/app_display.c` | Display hardware init (esp_lcd + esp_lvgl_port, Core 1, priority 1) |
| `main/modules/display/app_font.c` | Font manager - two-layer strategy: built-in basic fallback + CBin runtime from assets partition |
| `main/modules/display/ui/ui_manager.c` | Page manager - handles page switching and event-driven UI updates |
| `main/modules/display/ui/ui_home.c` | Home page UI (placeholder) |
| `main/modules/display/ui/ui_wifi_config.c` | WiFi config guidance page (AP SSID/password/URL) |
| `main/modules/wifi/app_wifi.c` | WiFi provisioning (AP+STA mode, auto-trigger, reconnection with exponential backoff) |
| `main/resources/html/index.html` | WiFi provisioning web page |

### Init Sequence (main.c)

Order matters: `NVS → event system → WiFi → display hardware → font manager → UI → main loop`

Font manager must init after display (LVGL core needed for `cbin_font_create`) and before UI (UI needs font pointers).

### Font System (Two-Layer)

- **Layer 1**: `font_puhui_basic_16_4` compiled into firmware (~220KB), always available as fallback
- **Layer 2**: `font_puhui_common_16_4.bin` loaded from assets partition via `esp_mmap_assets` + `cbin_font_create()`, full CJK coverage with 4bpp anti-aliasing
- All UI code uses `app_font_get_text()` / `app_font_get_icon()` from `app_font.h` — never reference fonts directly
- Assets partition packaged by `spiffs_create_partition_assets()` in `main/CMakeLists.txt`, flashed automatically with `idf.py flash`
- Auto-generated header `main/mmap_generate_assets.h` provides file indices and checksum — include it when adding asset access

### Event System

- **EventGroup** bits 0-9 for WiFi events, bit 23 for Schedule
- **Observer pattern**: modules register handlers with bit masks via `app_event_register_handler()`
- **Schedule**: `app_event_schedule(fn, arg)` queues deferred callbacks executed in main thread
- Main loop dispatches events by priority order (WiFi first, Schedule last), non-exclusive (multiple bits can fire simultaneously)

### Partition Table

Current 8MB layout: `nvs (24KB) → phy_init (4KB) → factory (3MB app) → assets (4.9MB)`. Changing to 16MB for OTA requires updating `partitions.csv` to dual-OTA layout + `sdkconfig` FLASHSIZE setting — no code changes needed.

### Display Stack

- **LCD Driver**: ESP-IDF built-in `esp_lcd_new_panel_st7789()` via `esp_lcd` framework
- **LVGL Adapter**: `esp_lvgl_port` component (managed component v2.7.x)
- **SPI**: `SPI2_HOST` @ 80MHz, DMA auto, polling mode
- **Buffer**: Partial render, `width * 20` pixels, single buffer, DMA capable
- **Thread Safety**: `lvgl_port_lock()` / `lvgl_port_unlock()` for cross-task LVGL access

### WiFi Provisioning

AP config (SSID prefix, password, channel) is defined in board config. AP SSID is dynamically generated: `prefix + MAC last 4 hex chars`. Auto-provisioning: tries saved WiFi first, 60s timeout → config mode. Reconnection: infinite retry with exponential backoff (10s→300s cap) after initial provisioning. HTTP endpoints: `/` `/scan` `/connect` `/disconnect` `/status`.

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
- 提交信息中不要出现其他项目名称

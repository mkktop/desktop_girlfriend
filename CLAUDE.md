# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
idf.py build                          # Build the project
idf.py -p <PORT> flash                # Flash to device
idf.py -p <PORT> monitor              # Serial monitor
idf.py -p <PORT> flash monitor        # Build, flash, and monitor
idf.py menuconfig                     # Configure SDK options
idf.py fullclean                      # Clean all build artifacts
```

## Architecture Overview

This is an ESP32-S3 embedded GUI project using LVGL for a "desktop girlfriend" device.

### Technology Stack
- **Framework**: ESP-IDF v5.4.1
- **MCU**: ESP32-S3 (dual-core)
- **Graphics**: LVGL v9.6.0
- **Display**: ST7789 (240×320, SPI @ 80MHz)
- **RTOS**: FreeRTOS

### Module Structure

| Path | Purpose |
|------|---------|
| `main/main.c` | Application entry point - initializes NVS, WiFi, LVGL |
| `main/modules/lvgl/app_lvgl.c` | Display init using esp_lcd + esp_lvgl_port (Core 1, priority 1, 6KB stack) |
| `main/modules/wifi/bsp_wifi_web.c` | WiFi provisioning (AP+STA mode) with embedded HTTP server |
| `main/resources/html/index.html` | WiFi provisioning web page |

### Display Stack

- **LCD Driver**: ESP-IDF built-in `esp_lcd_new_panel_st7789()` via `esp_lcd` framework
- **LVGL Adapter**: `esp_lvgl_port` component (managed component v2.7.x)
- **SPI**: `SPI2_HOST` @ 80MHz, DMA auto, polling mode
- **Buffer**: Partial render, `width * 20` pixels, single buffer, DMA capable
- **Thread Safety**: `lvgl_port_lock()` / `lvgl_port_unlock()` for cross-task LVGL access

### Hardware Pins (LCD)

| Pin | GPIO |
|-----|------|
| DC | GPIO 7 |
| RST | GPIO 8 |
| SCK | GPIO 9 |
| SDA (MOSI) | GPIO 10 |
| PWR (Backlight) | GPIO 11 |
| CS | GPIO 12 |

### WiFi Provisioning

The device starts in AP+STA mode with SSID "mkk_wifi". HTTP endpoints:
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

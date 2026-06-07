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
| `main/boards/<board_name>/config.json` | CI 构建变体定义（target、sdkconfig 追加项） |
| `main/boards/common/` | 板卡共享驱动（XL9555 IO 扩展、背光、按钮等） |
| `main/Kconfig.projbuild` | 板卡选择菜单（`menuconfig` 中可见） |

`board_t` 通过嵌套结构体分组外设配置：`lcd_cfg_t lcd`、`wifi_ap_cfg_t wifi_ap`、`font_cfg_t font`，扩展时只需添加新的 `xxx_cfg_t`。

`lcd_cfg_t` 支持两种 RST/背光控制模式：
- **直接 GPIO**（`use_io_expander = false`）：引脚直接连接 MCU
- **IO 扩展芯片**（`use_io_expander = true`）：通过 XL9555/TCA9555 I2C IO 扩展间接控制

添加新板卡只需：
1. 创建 `main/boards/<新板卡名>/board.c`，填充 `board_t` 结构体
2. 创建 `main/boards/<新板卡名>/config.json`，定义 CI 构建变体
3. 在 `Kconfig.projbuild` 添加选项
4. 在 `main/CMakeLists.txt` 添加 `elseif` 分支

### Module Structure

| Path | Purpose |
|------|---------|
| `main/main.c` | Application entry point - initializes modules, runs EventGroup-driven main loop at priority 10 |
| `main/modules/event/app_event.c` | Event system: FreeRTOS EventGroup + observer pattern + Schedule (deferred callbacks) |
| `main/modules/display/app_display.c` | Display hardware init (esp_lcd + esp_lvgl_port, Core 1, priority 1) |
| `main/modules/display/app_font.c` | Font manager - two-layer strategy: built-in basic fallback + CBin runtime from assets partition |
| `main/modules/display/gif/gifdec.c` | Pure C GIF89a decoder (replaces buggy lv_gif, fixes frame corruption) |
| `main/modules/display/gif/app_gif.c` | GIF animation wrapper for LVGL |
| `main/modules/display/ui/ui_manager.c` | Page manager - three-layer container architecture, page switching, event dispatch |
| `main/modules/display/ui/ui_home.c` | Home page UI (GIF animated emoji) |
| `main/modules/display/ui/ui_wifi_config.c` | WiFi config guidance page (AP SSID/password/URL) |
| `main/modules/sntp/app_sntp.c` | SNTP time sync (ntp.aliyun.com, auto-start on GOT_IP) |
| `main/modules/wifi/app_wifi.c` | WiFi provisioning (AP+STA mode, auto-trigger, reconnection with exponential backoff) |
| `main/resources/html/index.html` | WiFi provisioning web page |

### Init Sequence (main.c)

Order matters: `NVS → event system → SNTP → WiFi → display hardware → font manager → UI → main loop`

Font manager must init after display (LVGL core needed for `cbin_font_create`) and before UI (UI needs font pointers).

### UI Page System (Three-Layer Container)

```
lv_screen_active()
  └── sys_container（系统层，永不销毁）
        ├── status_bar（WiFi 状态 + 时钟，始终可见）
        └── page_container（页面层，承载当前功能页）
              └── [各功能页的组件作为子对象]
```

- **sys_container**: 全屏容器，页面切换不影响系统层
- **page_container**: `lv_obj_clean()` 一行清空旧页面，新页面在容器下创建
- **页面接口**: `ui_page_interface_t` — 每个页面实现 `create(parent)` + 可选 `on_event(bits)`
- **页面注册**: `s_pages[]` 数组，`ui_page_id_t` 枚举

添加新页面只需三步：
1. 创建 `ui_xxx.c/h`，实现 `ui_xxx_create(lv_obj_t *parent)`
2. 在 `ui_page_id_t` 枚举加新 ID
3. 在 `s_pages[]` 注册

未来扩展聊天浮层时，在 `sys_container` 下加 `overlay_container` 层即可。

### Font System (Two-Layer)

- **Layer 1**: `font_puhui_basic_16_4` compiled into firmware (~220KB), always available as fallback
- **Layer 2**: `font_puhui_common_16_4.bin` loaded from assets partition via `esp_mmap_assets` + `cbin_font_create()`, full CJK coverage with 4bpp anti-aliasing
- All UI code uses `app_font_get_text()` / `app_font_get_icon()` from `app_font.h` — never reference fonts directly
- Assets partition packaged by `spiffs_create_partition_assets()` in `main/CMakeLists.txt`, flashed automatically with `idf.py flash`
- Auto-generated header `main/mmap_generate_assets.h` provides file indices and checksum — include it when adding asset access

### Event System

- **EventGroup** bits 0-9 for WiFi events, bit 10 for time sync, bit 23 for Schedule
- **Observer pattern**: modules register handlers with bit masks via `app_event_register_handler()`
- **Schedule**: `app_event_schedule(fn, arg)` queues deferred callbacks executed in main thread
- Main loop dispatches events by priority order (WiFi first, Schedule last), non-exclusive (multiple bits can fire simultaneously)

### Partition Table

**8MB 布局**（`partitions.csv`）：`nvs (24KB) → phy_init (4KB) → factory (3MB app) → assets (4.9MB)`

**16MB OTA 布局**（`partitions_16mb.csv`）：`nvs → phy_init → factory (3MB) → otadata → ota_0 (3MB) → ota_1 (3MB) → assets (~6.9MB)`

16MB 板卡通过 `config.json` 的 `sdkconfig_append` 指定分区表，无需改代码。

### Display Stack

- **LCD Driver**: ESP-IDF built-in `esp_lcd_new_panel_st7789()` via `esp_lcd` framework
- **LVGL Adapter**: `esp_lvgl_port` component (managed component v2.7.x)
- **SPI**: `SPI2_HOST` @ 80MHz, DMA auto, polling mode
- **Buffer**: Partial render, `width * 20` pixels, single buffer, DMA capable
- **Thread Safety**: `lvgl_port_lock()` / `lvgl_port_unlock()` for cross-task LVGL access
- **IO 扩展模式**: `use_io_expander = true` 时，先初始化 XL9555，再通过 I2C 控制 RST/背光

### WiFi Provisioning

AP config (SSID prefix, password, channel) is defined in board config. AP SSID is dynamically generated: `prefix + MAC last 4 hex chars`. Auto-provisioning: tries saved WiFi first, 60s timeout → config mode. Reconnection: infinite retry with exponential backoff (10s→300s cap) after initial provisioning. HTTP endpoints: `/` `/scan` `/connect` `/disconnect` `/status`.

### CI/CD

GitHub Actions 两阶段流水线（`.github/workflows/build.yml`）：

- **触发条件**：`v*` tag 推送（编译 + Release）、PR 到 main（仅编译验证）
- **prepare**：扫描 `main/boards/*/config.json` 生成构建矩阵，PR 时按变更文件筛选受影响板卡
- **build**：矩阵构建，每个板卡用 `release.py` 编译 + merge-bin
- **release**：仅 tag 触发，产物命名 `板卡名_v版本号.bin`，通过 `softprops/action-gh-release` 发布

本地多板卡编译：`python scripts/release.py all`

版本号在根 `CMakeLists.txt` 的 `set(PROJECT_VER "x.y.z")`，发版流程：改版本号 → 提交 → `git tag vX.Y.Z` → `git push --tags`

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

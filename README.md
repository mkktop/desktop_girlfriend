# Desktop Girlfriend

基于 ESP32-S3 的嵌入式 GUI 桌面女友项目，使用 LVGL 图形库驱动 ST7789 TFT LCD 显示屏。

## 硬件配置

| 组件 | 型号/参数 |
|------|-----------|
| 主控芯片 | ESP32-S3（双核 240MHz，OCT PSRAM） |
| Flash | 8MB（当前开发板）/ 16MB（正式板，支持 OTA） |
| 显示屏 | ST7789（240×320，SPI @ 80MHz） |
| 通信接口 | SPI（DMA 异步传输） |

## 引脚连接

引脚定义集中在 `main/boards/desktop_girlfriend_V1/board.c`，当前板卡配置：

| LCD 引脚 | GPIO |
|----------|------|
| DC | GPIO 7 |
| RST | GPIO 8 |
| SCK | GPIO 9 |
| SDA (MOSI) | GPIO 10 |
| PWR (Backlight) | GPIO 11 |
| CS | GPIO 12 |

## 项目结构

```
├── CMakeLists.txt              # 根 CMake 配置
├── partitions.csv              # 自定义分区表（NVS + Factory + Assets）
├── sdkconfig                   # SDK 配置
├── main/
│   ├── CMakeLists.txt          # 板卡条件编译 + 组件注册 + assets 打包
│   ├── Kconfig.projbuild       # 板卡选择菜单
│   ├── idf_component.yml       # 组件依赖（lvgl, esp_lvgl_port, xiaozhi-fonts, esp_mmap_assets）
│   ├── main.c                  # 应用入口（EventGroup 驱动主循环）
│   ├── boards/                 # 板卡抽象层
│   │   ├── board.h             # 板卡配置结构体（嵌套分组）+ 单例接口
│   │   ├── common/             # 板卡共享驱动（背光、按钮等）
│   │   └── desktop_girlfriend_V1/  # 当前板卡
│   │       └── board.c         # LCD、WiFi AP、字体等外设配置
│   ├── modules/
│   │   ├── event/
│   │   │   └── app_event.c/h   # FreeRTOS EventGroup + 观察者模式 + Schedule 延迟回调
│   │   ├── display/
│   │   │   ├── app_display.c/h # 显示硬件初始化（esp_lcd + esp_lvgl_port）
│   │   │   ├── app_font.c/h    # 字体管理器（两层字体策略）
│   │   │   └── ui/
│   │   │       ├── ui_manager.c/h     # 页面管理器（首页 ↔ 配网引导页切换）
│   │   │       ├── ui_home.c/h        # 首页 UI
│   │   │       └── ui_wifi_config.c/h # WiFi 配网引导页（AP名称/密码/URL）
│   │   └── wifi/
│   │       └── app_wifi.c/h    # WiFi 配网（AP+STA + HTTP 服务器 + 自动配网 + 重连策略）
│   └── resources/
│       └── html/
│           └── index.html      # 配网网页（embed 进固件）
└── managed_components/         # IDF 自动管理的组件
    ├── lvgl__lvgl/             # LVGL v9.5.0
    ├── espressif__esp_lvgl_port/ # esp_lvgl_port v2.7.x
    ├── 78__xiaozhi-fonts/      # 阿里巴巴普惠体 + CBin 字体 + FontAwesome 图标
    └── espressif__esp_mmap_assets/ # 资源分区管理（mmap 零拷贝）
```

## 分区表

8MB Flash 布局（当前开发板）：

| 分区 | 类型 | 偏移 | 大小 | 说明 |
|------|------|------|------|------|
| nvs | data/nvs | 0x9000 | 24KB | WiFi 凭据等持久数据 |
| phy_init | data/phy | 0xF000 | 4KB | 射频校准 |
| factory | app/factory | 0x10000 | 3MB | 固件（当前 ~1.2MB，剩余 60%） |
| assets | data/spiffs | 0x310000 | 4.9MB | 字体、图片等资源（mmap 零拷贝） |

16MB Flash 布局（正式板，支持 OTA）：

| 分区 | 类型 | 偏移 | 大小 | 说明 |
|------|------|------|------|------|
| nvs | data/nvs | 0x9000 | 16KB | 持久数据 |
| otadata | data/ota | 0xD000 | 8KB | OTA 状态（A/B 槽位） |
| phy_init | data/phy | 0xF000 | 4KB | 射频校准 |
| ota_0 | app/ota_0 | 0x20000 | ~4MB | 固件分区 A |
| ota_1 | app/ota_1 | 自动 | ~4MB | 固件分区 B（OTA 备份） |
| assets | data/spiffs | 0x800000 | 8MB | 字体、图片、模型等资源 |

## 板卡抽象层

项目支持多板卡适配，通过编译时 Kconfig 选择：

1. `main/boards/board.h` — 定义 `board_t` 配置结构体，通过嵌套结构体分组：
   - `lcd_cfg_t lcd` — LCD 引脚、SPI 参数、显示参数
   - `wifi_ap_cfg_t wifi_ap` — WiFi 配网热点参数
   - `font_cfg_t font` — 字体配置（内置 + CBin 运行时）
   - 未来可扩展 `audio_cfg_t`、`touch_cfg_t`、`button_cfg_t` 等
2. `main/boards/<板卡名>/board.c` — 各板卡填充具体参数
3. `main/boards/common/` — 板卡共享驱动（PWM 背光、按钮、音频等）
4. `main/Kconfig.projbuild` — `menuconfig` 中的板卡选择菜单
5. `main/CMakeLists.txt` — 根据选择编译对应 `board.c`

**添加新板卡只需三步**：
1. 新建 `main/boards/<新板卡名>/board.c`，填充 `board_t`
2. 在 `Kconfig.projbuild` 添加选项
3. 在 `CMakeLists.txt` 添加 `elseif` 分支

## 字体系统

采用两层字体策略（与小智一致）：

| 层级 | 字体 | 来源 | 说明 |
|------|------|------|------|
| Layer 1 | `font_puhui_basic_16_4` | 编译时链接（~220KB） | ASCII + 基础 CJK，始终可用作为 fallback |
| Layer 2 | `font_puhui_common_16_4.bin` | 运行时从 assets 分区 mmap 加载（~864KB） | 完整常用 CJK，4bpp 抗锯齿 |

- **内置字体**：编译进固件，确保设备始终有可用文字
- **CBin 字体**：从 Flash 资源分区零拷贝映射，支持 OTA 独立更新
- **FontAwesome 图标**：`font_awesome_16_4`，用于状态图标
- **扩展性**：assets 分区可存放自定义表情图片、音频等资源

## 事件系统

基于 FreeRTOS EventGroup 驱动：

| 事件位 | 宏名 | 说明 |
|--------|------|------|
| 0-2 | `WIFI_CONNECTED / DISCONNECTED / GOT_IP` | WiFi 连接状态 |
| 3-5 | `AP_START / AP_STOP / STA_START` | WiFi 模式切换 |
| 6-7 | `SCANNING / CONNECTING` | WiFi 操作状态 |
| 8-9 | `CONFIG_ENTER / CONFIG_EXIT` | 配网模式进出 |
| 23 | `SCHEDULE_PENDING` | 延迟回调队列 |

- **主循环**以优先级 10 运行，`xEventGroupWaitBits` 阻塞等待
- **观察者模式**：模块通过 `app_event_register_handler()` 注册回调，按事件位掩码过滤
- **Schedule 机制**：`app_event_schedule()` 提交延迟回调到主线程执行，避免跨任务并发

## WiFi 配网

### 自动配网流程

1. 启动时读取 NVS 保存的 WiFi 配置
2. 有保存配置 → 尝试连接，60 秒超时 → 失败进入配网模式
3. 无保存配置 → 直接进入配网模式
4. 配网成功后断线 → 无限重试 + 指数退避（10s→20s→40s→...→300s 封顶）

### 配网引导

进入配网模式时屏幕显示：
- WiFi 图标 + "配网模式" 标题
- AP 热点名称（动态生成：`desktop_girlfriend-XXXX`）
- AP 密码
- 浏览器访问地址（`http://192.168.4.1`）

### HTTP 端点

| 端点 | 方法 | 功能 |
|------|------|------|
| `/` | GET | 配网网页 |
| `/scan` | GET | 扫描 WiFi |
| `/connect` | POST | 连接 WiFi（JSON） |
| `/disconnect` | POST | 断开 WiFi |
| `/status` | GET | 查询状态 |

## 技术栈

| 组件 | 版本/说明 |
|------|----------|
| ESP-IDF | v5.4.4 |
| LVGL | v9.5.0（managed component） |
| esp_lvgl_port | ~2.7.2（managed component） |
| xiaozhi-fonts | ~1.6.0（阿里巴巴普惠体 + CBin 加载器） |
| esp_mmap_assets | ~1.4.0（资源分区打包 + mmap 零拷贝） |
| LCD 驱动 | ESP-IDF 内置 `esp_lcd_new_panel_st7789()` |
| FreeRTOS | ESP-IDF 内置 |

## 快速开始

### 环境要求

- ESP-IDF v5.4+
- Python 3.8+

### 编译与烧录

**ESP-IDF 终端（CMD/PowerShell）：**

```bash
idf.py build                          # 编译
idf.py -p <PORT> flash                # 烧录（含 assets 分区）
idf.py -p <PORT> flash monitor        # 编译、烧录、监控
idf.py menuconfig                     # 配置（含板卡选择）
```

**Claude Code / Git Bash（通过 build.bat）：**

```bash
cmd.exe //C "build.bat build"                  # 编译
cmd.exe //C "build.bat -p COM5 flash"           # 烧录
rm -rf build && cmd.exe //C "build.bat build"   # 清理后编译
```

## 功能特性

- [x] ST7789 LCD 显示（esp_lcd 框架）
- [x] LVGL 图形界面（esp_lvgl_port 托管）
- [x] WiFi 配网（AP+STA + 网页配置 + 自动配网触发）
- [x] WiFi 配网引导界面（AP名称/密码/URL 显示）
- [x] WiFi 重连策略（无限重试 + 指数退避）
- [x] DMA 异步传输
- [x] PSRAM 图像缓存优化
- [x] 板卡抽象层（支持多板卡）
- [x] 事件系统（FreeRTOS EventGroup + 观察者模式）
- [x] 两层字体策略（内置 fallback + CBin 运行时加载）
- [x] 资源分区管理（esp_mmap_assets mmap 零拷贝）
- [x] 动态 AP SSID（前缀 + MAC 地址）
- [ ] OTA 固件升级（16MB 正式板）
- [ ] 触摸输入支持
- [ ] AI 对话功能
- [ ] 自定义表情

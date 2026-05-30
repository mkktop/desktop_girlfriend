# ESP32-S3 桌面女友项目

基于 ESP32-S3 的嵌入式 GUI 项目，使用 LVGL 图形库驱动 ST7789 TFT LCD 显示屏。

## 硬件配置

| 组件 | 型号/参数 |
|------|-----------|
| 主控芯片 | ESP32-S3（双核 240MHz，OCT PSRAM） |
| 显示屏 | ST7789（240×320，SPI @ 80MHz） |
| 通信接口 | SPI（DMA 异步传输） |

## 引脚连接

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
├── partitions.csv              # 自定义分区表（NVS 24KB + Factory 3MB）
├── sdkconfig                   # SDK 配置
├── main/
│   ├── CMakeLists.txt
│   ├── idf_component.yml       # 组件依赖（lvgl ~9.5.0, esp_lvgl_port ~2.7.2）
│   ├── main.c                  # 应用入口
│   ├── modules/
│   │   ├── lvgl/
│   │   │   ├── app_lvgl.c      # 显示初始化（esp_lcd + esp_lvgl_port）
│   │   │   └── app_lvgl.h
│   │   └── wifi/
│   │       ├── bsp_wifi_web.c  # WiFi 配网（AP+STA + HTTP 服务器）
│   │       └── bsp_wifi_web.h
│   └── resources/
│       └── html/
│           └── index.html      # 配网网页（embed 进固件）
└── managed_components/         # IDF 自动管理的组件
    ├── lvgl__lvgl/             # LVGL v9.5.0
    └── espressif__esp_lvgl_port/ # esp_lvgl_port v2.7.x
```

## 技术栈

| 组件 | 版本/说明 |
|------|----------|
| ESP-IDF | v5.4.4 |
| LVGL | v9.5.0（managed component） |
| esp_lvgl_port | ~2.7.2（managed component） |
| LCD 驱动 | ESP-IDF 内置 `esp_lcd_new_panel_st7789()` |
| FreeRTOS | ESP-IDF 内置 |

## 显示架构

```
app_lvgl_init()
  ├─ spi_bus_initialize(SPI2_HOST)         ← SPI 总线
  ├─ esp_lcd_new_panel_io_spi()            ← Panel IO（SPI）
  ├─ esp_lcd_new_panel_st7789()            ← ST7789 面板（IDF 内置驱动）
  ├─ esp_lcd_panel_init() + invert + 清屏  ← 面板初始化
  ├─ lv_init() + lv_image_cache_resize()   ← LVGL 核心 + PSRAM 图像缓存
  ├─ lvgl_port_init()                      ← LVGL 端口（自动创建任务/tick/锁）
  └─ lvgl_port_add_disp()                  ← 添加显示设备（DMA 异步 flush）
```

### FreeRTOS 调度

| 任务 | 栈大小 | 优先级 | 核心 | 说明 |
|------|--------|--------|------|------|
| LVGL 任务 | 6144 | 1 | Core 1 | esp_lvgl_port 自动管理，mutex 保护 |
| 主任务 | 默认 | 默认 | Core 0 | WiFi 配网 + 主循环 |

### 缓冲区配置

| 参数 | 值 |
|------|-----|
| 渲染模式 | Partial |
| Buffer 大小 | 240 × 20 = 4,800 像素 |
| Buffer 数量 | 1（单 buffer） |
| DMA | ✅ 启用 |
| 字节交换 | SPI 层 swap_bytes=1 |

## WiFi 配网

设备启动后以 AP+STA 模式运行，SSID：`mkk_wifi`。

| HTTP 端点 | 方法 | 功能 |
|-----------|------|------|
| `/` | GET | 配网网页 |
| `/scan` | GET | 扫描 WiFi |
| `/connect` | POST | 连接 WiFi |
| `/disconnect` | POST | 断开 WiFi |
| `/status` | GET | 查询状态 |

## 快速开始

### 环境要求

- ESP-IDF v5.4+
- Python 3.8+

### 编译与烧录

```bash
idf.py build                          # 编译
idf.py -p <PORT> flash                # 烧录
idf.py -p <PORT> flash monitor        # 编译、烧录、监控
```

## 功能特性

- [x] ST7789 LCD 显示（esp_lcd 框架）
- [x] LVGL 图形界面（esp_lvgl_port 托管）
- [x] WiFi 配网（AP+STA + 网页配置）
- [x] DMA 异步传输
- [x] PSRAM 图像缓存优化
- [ ] 触摸输入支持
- [ ] AI 对话功能

# ESP32-S3 LVGL 桌面女友项目

基于 ESP32-S3 的嵌入式 GUI 项目，使用 LVGL 作为图形库，驱动 ST7789 TFT LCD 显示屏。

## 硬件配置

| 组件 | 型号/参数 |
|------|-----------|
| 主控芯片 | ESP32-S3 |
| 显示屏 | ST7789 (240×320) |
| 通信接口 | SPI |

## 引脚连接

| LCD引脚 | GPIO |
|---------|------|
| DC      | GPIO 7 |
| RST     | GPIO 8 |
| SCK     | GPIO 9 |
| SDA     | GPIO 10 |
| PWR     | GPIO 11 |
| CS      | GPIO 12 |

## 项目结构

```
├── CMakeLists.txt              # 根CMake配置
├── main/
│   ├── CMakeLists.txt
│   ├── main.c                  # 应用入口
│   ├── app_lvgl.c              # LVGL任务实现
│   └── app_lvgl.h              # LVGL头文件
├── lvgl/                       # LVGL图形库
│   ├── lv_conf.h               # LVGL配置
│   ├── lv_port_disp.c          # 显示驱动端口
│   └── src/                    # LVGL源码
├── Driver/
│   └── LCD/
│       ├── st7789.c            # ST7789驱动
│       └── st7789.h
└── README.md
```

## 软件架构

- **LVGL v9.6.0** - 图形库
- **ESP-IDF** - 开发框架
- **FreeRTOS** - 操作系统

### 任务划分

| 任务 | 栈大小 | 优先级 | 核心 |
|------|--------|--------|------|
| lvgl_task | 8192 | 5 | Core 0 |
| 主任务 | - | - | Core 0 |

## 快速开始

### 环境要求

- ESP-IDF v5.0+
- Python 3.8+
- CMake 3.5+

### 编译

```bash
idf.py build
```

### 烧录

```bash
idf.py -p <PORT> flash
```

### 监控

```bash
idf.py -p <PORT> monitor
```

## 功能特性

- [x] ST7789 LCD 驱动
- [x] LVGL 图形界面
- [x] LVGL 独立任务运行
- [ ] 触摸输入支持
- [ ] 文件系统支持
- [ ] 网络功能

## 注意事项

- LVGL 运行在独立任务中，避免阻塞主线程
- 显示缓冲区采用双缓冲 Partial 模式
- LVGL tick 由硬件定时器驱动 (1ms 周期)

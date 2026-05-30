# boards/common/

此目录存放板卡共享的硬件驱动模块。

每个驱动一个 `.c/.h` 文件对，由各板卡的 `board.c` 按需引用。

## 未来计划

| 模块 | 说明 |
|------|------|
| `app_backlight.c/h` | PWM 背光控制（渐亮渐灭、亮度调节） |
| `app_button.c/h` | 按钮驱动（去抖、长按、短按） |
| `app_audio.c/h` | I2S 音频驱动（MIC + 喇叭） |
| `app_touch.c/h` | 触摸屏 I2C 驱动 |

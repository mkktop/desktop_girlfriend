/**
 * @file xl9555.h
 * @brief XL9555 I2C IO 扩展芯片驱动
 * @author mkk
 * @date 2026-06-06
 * @note 通用驱动，适用于所有使用 XL9555/TCA9555 系列 IO 扩展芯片的板卡。
 *       引脚编号使用 16 位 bitmask：
 *         bit 0-7  → P0 端口（P00~P07）
 *         bit 8-15 → P1 端口（P10~P17）
 *       初始化时通过 output_mask 指定输出引脚，其余自动配置为输入。
 */

#ifndef __XL9555_H__
#define __XL9555_H__

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 初始化 XL9555 IO 扩展芯片
 * @param sda_pin I2C SDA 引脚号
 * @param scl_pin I2C SCL 引脚号
 * @param addr 7 位 I2C 器件地址（通常为 0x20）
 * @param output_mask 输出引脚 bitmask（对应位=1 为输出，其余为输入）
 * @return 0 成功，-1 失败
 * @note 例如 LCD RST(P12) + 背光(P13) 作为输出：output_mask = 0x0C00
 */
int xl9555_init(int sda_pin, int scl_pin, uint8_t addr, uint16_t output_mask);

/**
 * @brief 设置 XL9555 输出引脚电平
 * @param pin 引脚 bitmask（如 0x0400 表示 P12）
 * @param val 0=低电平，1=高电平
 */
void xl9555_pin_write(uint16_t pin, int val);

/**
 * @brief 读取 XL9555 引脚电平
 * @param pin 引脚 bitmask
 * @return 0 或 1
 */
int xl9555_pin_read(uint16_t pin);

/**
 * @brief 检查 XL9555 是否已初始化
 * @return true 已初始化
 */
bool xl9555_is_initialized(void);

#endif /* __XL9555_H__ */

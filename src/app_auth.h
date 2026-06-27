/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef APP_AUTH_H__
#define APP_AUTH_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化认证模块。
 *        计算默认密码 password_hash = SHA256("123456" + SN + SALT)。
 *        启动后 BLE 和 NFC 均为锁定状态。
 *
 *        【密码持久化】本测试工程暂不将 password_hash 写入 Flash，
 *        修改密码仅保存在 RAM 中，设备重启后恢复为默认密码 "123456"。
 *        如需完整测试修改密码，应在此函数中先从 Zephyr settings 或
 *        Flash KV 加载已保存的 password_hash，不存在时再 fallback 到默认密码。
 */
void app_auth_init(void);

/**
 * @brief 处理 CMD 0x03 认证与密码管理命令。
 *
 * @param data    收到的命令 payload（含 CMD 字节），如 [03, subcmd, args...]
 * @param len     payload 长度
 * @param resp    输出响应缓冲区
 * @param channel 通道名称："BLE" 或 "NFC"
 * @return 响应字节数（写入 resp），0 表示不是 CMD 0x03
 */
uint16_t app_auth_handle_cmd(const uint8_t *data, uint16_t len,
			     uint8_t *resp, const char *channel);

/**
 * @brief 检查当前通道是否已解锁（允许写操作）。
 *
 * @param channel "BLE" 或 "NFC"
 * @return 0x00 已解锁，否则为错误码：
 *         - 0x0A AUTH_FAILED
 *         - 0x0B AUTH_REQUIRED
 *         - 0x0C AUTH_EXPIRED
 */
uint8_t app_auth_check_write(const char *channel);

/** BLE 断开连接时调用，清除 BLE 解锁状态 */
void app_auth_ble_disconnect(void);

/** 恢复出厂时调用，重置密码 hash 为默认值 "123456" */
void app_auth_reset_password(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_AUTH_H__ */

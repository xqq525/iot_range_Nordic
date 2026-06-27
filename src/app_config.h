/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef APP_CONFIG_H__
#define APP_CONFIG_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* V2.0 协议常量（含向后兼容别名） */
#include "app_protocol.h"

/** External Type 字符串 */
#define APP_CONFIG_CMD_TYPE      "ulpinternal:cmd"
#define APP_CONFIG_CMD_TYPE_LEN  15
#define APP_CONFIG_INFO_TYPE     "ulpinternal:info"
#define APP_CONFIG_INFO_TYPE_LEN 16

/** OTA 调试缓冲区（RAM，32KB）*/
#define APP_CONFIG_OTA_BUF_SIZE  (32 * 1024)
#define APP_CONFIG_OTA_PKT_DATA  492

struct app_config_ota_info {
	bool     active;
	bool     done;
	uint32_t total_size;
	uint16_t packets_received;
	uint32_t bytes_received;
};

void app_config_ota_get_info(struct app_config_ota_info *info);
const uint8_t *app_config_ota_get_buf(void);

/* 传感器数据更新接口 */
void app_config_set_battery(uint8_t level);
void app_config_set_temperature(int16_t temp);
void app_config_set_distance(uint16_t dist);
void app_config_set_position(uint8_t pos);
void app_config_set_mode(uint8_t mode);
void app_config_set_update_time(uint16_t interval);
void app_config_set_gnss_fix(uint8_t fix);
void app_config_set_latitude(int32_t lat);
void app_config_set_longitude(int32_t lng);
void app_config_set_altitude(int16_t alt);

/* 全局时间戳（APP CMD 0x01 附带） */
uint32_t app_config_get_unixtime(void);

/**
 * @brief 处理收到的NDEF消息，解析命令并更新为应答数据帧。
 *
 *        在 NFC_T4T_EVENT_NDEF_UPDATED 事件中调用。
 *        如果识别为有效的配置命令，则将 ndef_msg_buf 替换为应答NDEF消息，
 *        并更新T4T载荷。
 *
 * @param ndef_msg_buf     NDEF文件缓冲区（含NLEN前缀）
 * @param ndef_msg_buf_size 缓冲区总大小
 * @return true  已识别命令并更新应答
 * @return false 不是配置命令，调用者按原有逻辑处理
 */
bool app_config_handle_ndef(uint8_t *ndef_msg_buf, size_t ndef_msg_buf_size);

/**
 * @brief 处理BLE收到的裸命令。
 *
 *        命令格式: [CMD, ...params]
 *        应答写入 resp 缓冲区，返回实际字节数。
 *
 * @param data   BLE收到的原始bytes
 * @param len    数据长度
 * @param resp   输出缓冲区，须>=APP_PROTO_RESP_MAX(256)字节
 * @return 实际应答字节数，0 表示无需回复（如 OTA 数据包）
 */
uint16_t app_config_handle_ble(const uint8_t *data, uint16_t len, uint8_t *resp);

#ifdef __cplusplus
}
#endif

#endif /* APP_CONFIG_H__ */

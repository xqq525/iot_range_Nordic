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

/** 命令码 */
#define APP_CONFIG_CMD_READ_INFO    0x01
#define APP_CONFIG_CMD_WRITE_CONFIG 0x02

/** CMD 0x02 参数子项 param_id */
#define APP_CONFIG_PARAM_MODE            0x01
#define APP_CONFIG_PARAM_REPORT_INTERVAL 0x02

/** 错误码 */
#define APP_CONFIG_STATUS_OK            0x00
#define APP_CONFIG_STATUS_UNSUPPORTED   0x01
#define APP_CONFIG_STATUS_BUSY          0x02
#define APP_CONFIG_STATUS_UNKNOWN       0xFF

/** 应答Payload定长 */
#define APP_CONFIG_RESPONSE_PAYLOAD_SIZE 50

/** External Type 字符串 */
#define APP_CONFIG_CMD_TYPE      "ulpinternal:cmd"
#define APP_CONFIG_CMD_TYPE_LEN  15
#define APP_CONFIG_INFO_TYPE     "ulpinternal:info"
#define APP_CONFIG_INFO_TYPE_LEN 16

/* 传感器数据更新接口 */
void app_config_set_battery(uint8_t level);
void app_config_set_temperature(int16_t temp);
void app_config_set_distance(uint16_t dist);
void app_config_set_position(uint8_t pos);
void app_config_set_mode(uint8_t mode);
void app_config_set_update_time(uint16_t interval);

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

#ifdef __cplusplus
}
#endif

#endif /* APP_CONFIG_H__ */

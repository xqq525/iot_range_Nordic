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
#define APP_CONFIG_CMD_OTA_START    0x10
#define APP_CONFIG_CMD_OTA_DATA     0x11
#define APP_CONFIG_CMD_OTA_END      0x12

/** CMD 0x02 参数子项 param_id */
#define APP_CONFIG_PARAM_MODE            0x01
#define APP_CONFIG_PARAM_REPORT_INTERVAL 0x02
#define APP_CONFIG_PARAM_INSTALL_POS     0x03
#define APP_CONFIG_PARAM_MODEL_CHECK     0x04

/** 错误码 */
#define APP_CONFIG_STATUS_OK            0x00
#define APP_CONFIG_STATUS_UNSUPPORTED   0x01
#define APP_CONFIG_STATUS_BUSY          0x02
#define APP_CONFIG_STATUS_OTA_DATA_LEN  0x03
#define APP_CONFIG_STATUS_OTA_IDX       0x04
#define APP_CONFIG_STATUS_MODEL_MISMATCH 0x05
#define APP_CONFIG_STATUS_UNKNOWN       0xFF

/** 应答Payload定长 */
#define APP_CONFIG_RESPONSE_PAYLOAD_SIZE 63
#define APP_CONFIG_DEVICE_TYPE           0x01

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
void app_config_set_gnss_fix(uint8_t fix);
void app_config_set_latitude(int32_t lat);
void app_config_set_longitude(int32_t lng);
void app_config_set_altitude(int16_t alt);

/* 设备固定信息 */
const char *app_config_get_model(void);
const char *app_config_get_sn(void);

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
 * @brief 处理BLE收到的裸命令，构建63字节应答payload。
 *
 *        命令格式: [CMD, ...params]
 *          CMD 0x01: [0x01, TS_3, TS_2, TS_1, TS_0] 5字节
 *          CMD 0x02: [0x02, param_id, value...] 变长
 *            param_id 0x01: mode (1字节)
 *            param_id 0x02: interval (2字节 BE)
 *            param_id 0x03: install_pos LAT(4字节 BE) + LNG(4字节 BE)
 *
 *        应答格式: 63字节，与NFC应答payload完全相同。
 *
 * @param data   BLE收到的原始bytes
 * @param len    数据长度
 * @param resp   输出缓冲区，须>=APP_CONFIG_RESPONSE_PAYLOAD_SIZE(63)字节
 * @return 实际应答字节数，0 表示无需回复（如 OTA 数据包）
 */
uint16_t app_config_handle_ble(const uint8_t *data, uint16_t len, uint8_t *resp);

#ifdef __cplusplus
}
#endif

#endif /* APP_CONFIG_H__ */

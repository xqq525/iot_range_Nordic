/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file app_protocol.h
 * @brief V2.0 协议常量定义。
 *
 * 本文件定义 V2.0 协议全部常量，包括命令码、错误码、FIELD_ID、
 * DEVICE_STATE、认证子命令等。同时提供向后兼容别名，供旧模块编译使用。
 */

#ifndef APP_PROTOCOL_H__
#define APP_PROTOCOL_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 命令码 ── */
#define APP_PROTO_CMD_READ       0x01
#define APP_PROTO_CMD_WRITE      0x02
#define APP_PROTO_CMD_AUTH       0x03
#define APP_PROTO_CMD_ACTION     0x04
#define APP_PROTO_CMD_OTA_START  0x10
#define APP_PROTO_CMD_OTA_DATA   0x11
#define APP_PROTO_CMD_OTA_END    0x12

/* ── 错误码（与协议文档 §14 对齐）── */
#define APP_PROTO_STATUS_OK                 0x00
#define APP_PROTO_STATUS_UNSUPPORTED        0x01
#define APP_PROTO_STATUS_BUSY               0x02
#define APP_PROTO_STATUS_OTA_DATA_LEN       0x03
#define APP_PROTO_STATUS_OTA_IDX            0x04
#define APP_PROTO_STATUS_PARAM_LEN          0x05
#define APP_PROTO_STATUS_PARAM_VALUE        0x06
#define APP_PROTO_STATUS_MODEL_MISMATCH     0x07
#define APP_PROTO_STATUS_CHANNEL_UNSUPPORTED 0x08
#define APP_PROTO_STATUS_OTA_STATE          0x09
#define APP_PROTO_STATUS_AUTH_FAILED        0x0A
#define APP_PROTO_STATUS_AUTH_REQUIRED      0x0B
#define APP_PROTO_STATUS_AUTH_EXPIRED       0x0C
#define APP_PROTO_STATUS_AUTH_STATE         0x0D
#define APP_PROTO_STATUS_PASSWORD_FORMAT    0x0E
#define APP_PROTO_STATUS_STATE_NOT_ALLOWED  0x0F
#define APP_PROTO_STATUS_UNKNOWN            0xFF

/* ── FIELD_ID（与协议文档 §8 对齐）── */

/* 设备基础字段 (R) */
#define FIELD_DEVICE_STATE        0x01
#define FIELD_DEVICE_TYPE         0x02
#define FIELD_MODEL               0x03
#define FIELD_SN                  0x04
#define FIELD_FW_VER              0x05
#define FIELD_HW_VER              0x06
#define FIELD_PROTOCOL_VERSION    0x07

/* 设备状态字段 (R) */
#define FIELD_BATTERY             0x10
#define FIELD_TEMPERATURE         0x11
#define FIELD_DISTANCE            0x12
#define FIELD_POSITION            0x13

/* 常规配置字段 */
#define FIELD_MODE                0x20  /* R/W */
#define FIELD_UPDATE_INTERVAL     0x21  /* R/W */
#define FIELD_INSTALL_LOCATION    0x22  /* R/W */
#define FIELD_GNSS_FIX            0x23  /* R   */
#define FIELD_LATITUDE            0x24  /* R   */
#define FIELD_LONGITUDE           0x25  /* R   */
#define FIELD_ALTITUDE            0x26  /* R   */

/* 网络/平台配置预留字段（仅宏定义，不注册到字段表） */
#define FIELD_NB_APN              0x30
#define FIELD_NB_USERNAME         0x31
#define FIELD_NB_PASSWORD         0x32
#define FIELD_PLATFORM_HOST       0x33
#define FIELD_PLATFORM_PORT       0x34
#define FIELD_PLATFORM_PATH       0x35
#define FIELD_REPORT_PROTOCOL     0x36

/* ── DEVICE_STATE ── */
#define DEVICE_STATE_FACTORY      0x00
#define DEVICE_STATE_POWER_ON     0x01
#define DEVICE_STATE_POWER_OFF    0x02

/* ── 认证子命令 ── */
#define AUTH_SUBCMD_NONCE         0x01
#define AUTH_SUBCMD_TOKEN         0x02
#define AUTH_SUBCMD_CHANGE_PWD    0x03
#define AUTH_SUBCMD_LOCK          0x04

/* ── 认证状态 ── */
#define AUTH_STATE_LOCKED         0x00
#define AUTH_STATE_UNLOCKED       0x01

/* ── 设备类型 ── */
/*
 * RL202601 测距系列产品型号对照：
 *   产品形态                  对外型号      协议 MODEL
 *   ToF + NB-IoT              RL-TN100      RL_TN100  ← 当前型号
 *   ToF + 超声波 + NB-IoT      RL-DN100      RL_DN100
 *   ToF + LoRa                RL-TL100      RL_TL100
 *   ToF + 超声波 + LoRa        RL-DL100      RL_DL100
 */
#define DEVICE_TYPE_RL           0x01

/* ── 协议版本值 ── */
#define PROTOCOL_VERSION_VALUE    0x20

/* ── Payload / 响应缓冲区上限 ── */
#define APP_PROTO_PAYLOAD_MAX     240
#define APP_PROTO_RESP_MAX        256

/* ── 统一协议处理入口 ── */

/**
 * @brief NFC/BLE 共用的业务 Payload 统一处理入口。
 *
 * @param channel   "BLE" 或 "NFC"
 * @param req       请求 payload（含 CMD 字节）
 * @param req_len   请求长度
 * @param resp      响应缓冲区
 * @param resp_max  响应缓冲区最大字节数
 * @return 实际写入 resp 的字节数，0 表示无需回复
 */
uint16_t process_app_payload(const char *channel,
			     const uint8_t *req, uint16_t req_len,
			     uint8_t *resp, uint16_t resp_max);

/* ═════════════════════════════════════════════════════════════════════
 * 向后兼容别名（供 shell_cmd.c / app_auth.c / app_config.c 编译使用）
 * ═════════════════════════════════════════════════════════════════════ */

/* 命令码 */
#define APP_CONFIG_CMD_READ_INFO      APP_PROTO_CMD_READ
#define APP_CONFIG_CMD_WRITE_CONFIG   APP_PROTO_CMD_WRITE
#define APP_CONFIG_CMD_AUTH           APP_PROTO_CMD_AUTH
#define APP_CONFIG_CMD_OTA_START      APP_PROTO_CMD_OTA_START
#define APP_CONFIG_CMD_OTA_DATA       APP_PROTO_CMD_OTA_DATA
#define APP_CONFIG_CMD_OTA_END        APP_PROTO_CMD_OTA_END

/* 错误码 */
#define APP_CONFIG_STATUS_OK               APP_PROTO_STATUS_OK
#define APP_CONFIG_STATUS_UNSUPPORTED      APP_PROTO_STATUS_UNSUPPORTED
#define APP_CONFIG_STATUS_BUSY             APP_PROTO_STATUS_BUSY
#define APP_CONFIG_STATUS_OTA_DATA_LEN     APP_PROTO_STATUS_OTA_DATA_LEN
#define APP_CONFIG_STATUS_OTA_IDX          APP_PROTO_STATUS_OTA_IDX
#define APP_CONFIG_STATUS_PARAM_LEN        APP_PROTO_STATUS_PARAM_LEN
#define APP_CONFIG_STATUS_PARAM_VALUE      APP_PROTO_STATUS_PARAM_VALUE
#define APP_CONFIG_STATUS_MODEL_MISMATCH   APP_PROTO_STATUS_MODEL_MISMATCH
#define APP_CONFIG_STATUS_CHANNEL_UNSUPPORTED APP_PROTO_STATUS_CHANNEL_UNSUPPORTED
#define APP_CONFIG_STATUS_OTA_STATE        APP_PROTO_STATUS_OTA_STATE
#define APP_CONFIG_STATUS_AUTH_FAILED      APP_PROTO_STATUS_AUTH_FAILED
#define APP_CONFIG_STATUS_AUTH_REQUIRED    APP_PROTO_STATUS_AUTH_REQUIRED
#define APP_CONFIG_STATUS_AUTH_EXPIRED     APP_PROTO_STATUS_AUTH_EXPIRED
#define APP_CONFIG_STATUS_AUTH_STATE       APP_PROTO_STATUS_AUTH_STATE
#define APP_CONFIG_STATUS_PASSWORD_FORMAT  APP_PROTO_STATUS_PASSWORD_FORMAT
#define APP_CONFIG_STATUS_UNKNOWN          APP_PROTO_STATUS_UNKNOWN

/* 认证子命令 */
#define APP_CONFIG_AUTH_SUBCMD_NONCE       AUTH_SUBCMD_NONCE
#define APP_CONFIG_AUTH_SUBCMD_TOKEN       AUTH_SUBCMD_TOKEN
#define APP_CONFIG_AUTH_SUBCMD_CHANGE_PWD  AUTH_SUBCMD_CHANGE_PWD
#define APP_CONFIG_AUTH_SUBCMD_LOCK        AUTH_SUBCMD_LOCK

/* 认证状态 */
#define APP_CONFIG_AUTH_STATE_LOCKED       AUTH_STATE_LOCKED
#define APP_CONFIG_AUTH_STATE_UNLOCKED     AUTH_STATE_UNLOCKED

/* 设备类型 */
#define APP_CONFIG_DEVICE_TYPE             DEVICE_TYPE_RL

/* 旧响应大小常量（仅用于 shell_cmd.c 编译，V2.0 实际使用 APP_PROTO_RESP_MAX） */
#define APP_CONFIG_RESPONSE_PAYLOAD_SIZE   APP_PROTO_RESP_MAX

#ifdef __cplusplus
}
#endif

#endif /* APP_PROTOCOL_H__ */

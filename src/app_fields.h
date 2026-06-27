/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file app_fields.h
 * @brief V2.0 字段注册表、读写、校验、设备状态机。
 *
 * 管理所有 FIELD_ID 的读写权限、值校验和存储。
 * 设备状态 RAM-only，重启回 FACTORY。
 */

#ifndef APP_FIELDS_H__
#define APP_FIELDS_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化所有字段为默认值，设备状态置为 FACTORY。
 */
void app_fields_init(void);

/**
 * @brief 读取字段值到缓冲区。
 *
 * @param field_id  字段 ID
 * @param buf       输出缓冲区
 * @param buf_max   缓冲区最大字节数
 * @return 实际写入字节数，0 表示字段不支持或不存在
 */
uint8_t app_field_get(uint8_t field_id, uint8_t *buf, uint8_t buf_max);

/**
 * @brief 校验字段值是否合法（不实际写入）。
 *
 * @param field_id  字段 ID
 * @param value     待校验值
 * @param len       值长度
 * @return 0 合法，非 0 为错误码（UNSUPPORTED / PARAM_LEN / PARAM_VALUE）
 */
uint8_t app_field_validate(uint8_t field_id, const uint8_t *value, uint8_t len);

/**
 * @brief 设置字段值（调用前必须已通过 validate）。
 *
 * @param field_id  字段 ID
 * @param value     新值
 * @param len       值长度
 */
void app_field_set(uint8_t field_id, const uint8_t *value, uint8_t len);

/**
 * @brief 获取当前设备状态。
 * @return DEVICE_STATE_FACTORY / DEVICE_STATE_POWER_ON / DEVICE_STATE_POWER_OFF
 */
uint8_t app_field_get_device_state(void);

/**
 * @brief 设置设备状态。
 */
void app_field_set_device_state(uint8_t state);

/**
 * @brief 检查出厂初始化是否已完成。
 * @return true 已完成，false 未完成
 */
bool app_field_is_factory_init_done(void);

/**
 * @brief 标记出厂初始化完成，若当前为 FACTORY 则切换到 POWER_ON。
 */
void app_field_mark_factory_init_done(void);

/**
 * @brief 设置全局时间戳（由 CMD 0x01 携带的 APP UTC 时间）。
 */
void app_field_set_unixtime(uint32_t ts);

/**
 * @brief 获取最后一次 CMD 0x01 携带的全局时间戳。
 */
uint32_t app_field_get_unixtime(void);

/* ── 传感器数据外部更新接口（供 app_config.c 和 shell 使用）── */
void app_field_set_battery(uint8_t level);
void app_field_set_temperature(int16_t temp);
void app_field_set_distance(uint16_t dist);
void app_field_set_position(uint8_t pos);
void app_field_set_gnss_fix(uint8_t fix);
void app_field_set_latitude(int32_t lat);
void app_field_set_longitude(int32_t lng);
void app_field_set_altitude(int16_t alt);

#ifdef __cplusplus
}
#endif

#endif /* APP_FIELDS_H__ */

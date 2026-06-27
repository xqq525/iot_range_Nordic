/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file app_tlv.h
 * @brief V2.0 TLV 编解码器。
 *
 * TLV 格式: [FIELD_ID(1), LEN(1), VALUE(LEN)]
 */

#ifndef APP_TLV_H__
#define APP_TLV_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 将单个字段编码为 TLV 格式写入缓冲区。
 *
 * @param buf        目标缓冲区
 * @param field_id   字段 ID
 * @param value      字段值
 * @param value_len  值长度
 * @return 写入字节数（= 2 + value_len）
 */
uint8_t tlv_encode_field(uint8_t *buf, uint8_t field_id,
			 const uint8_t *value, uint8_t value_len);

/**
 * @brief 从 payload 中解码一个 TLV 字段，自动推进 offset。
 *
 * @param data       源数据
 * @param data_len   源数据总长度
 * @param offset     当前解析偏移（入/出）
 * @param field_id   输出：字段 ID
 * @param value_len  输出：值长度
 * @param value      输出：指向值的指针（指向 data 内部）
 * @return 0 成功，-1 表示 FIELD_ID+LEN 越界，-2 表示 VALUE 越界
 */
int tlv_decode_field(const uint8_t *data, uint16_t data_len,
		     uint16_t *offset,
		     uint8_t *field_id, uint8_t *value_len,
		     const uint8_t **value);

/**
 * @brief 预扫描整个 TLV 列表，校验结构和字段数量。
 *
 *        检查：每个 TLV 的 VALUE 不越界、总条目数与 field_count 一致。
 *
 * @param data         TLV 数据（不含 CMD 字节和 FIELD_COUNT）
 * @param data_len     TLV 数据长度
 * @param field_count  期望字段数量
 * @return 0 成功，-1 表示结构错误
 */
int tlv_validate_payload(const uint8_t *data, uint16_t data_len,
			 uint8_t field_count);

#ifdef __cplusplus
}
#endif

#endif /* APP_TLV_H__ */

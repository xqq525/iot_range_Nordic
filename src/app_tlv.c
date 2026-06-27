/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "app_tlv.h"
#include <string.h>

uint8_t tlv_encode_field(uint8_t *buf, uint8_t field_id,
			 const uint8_t *value, uint8_t value_len)
{
	buf[0] = field_id;
	buf[1] = value_len;
	if (value_len > 0 && value != NULL) {
		memcpy(&buf[2], value, value_len);
	}
	return 2 + value_len;
}

int tlv_decode_field(const uint8_t *data, uint16_t data_len,
		     uint16_t *offset,
		     uint8_t *field_id, uint8_t *value_len,
		     const uint8_t **value)
{
	/* 需要至少 FIELD_ID + LEN = 2 字节 */
	if (*offset + 2 > data_len) {
		return -1;
	}

	*field_id  = data[*offset];
	*value_len = data[*offset + 1];
	*offset += 2;

	/* 检查 VALUE 是否越界 */
	if (*offset + *value_len > data_len) {
		return -2;
	}

	*value = &data[*offset];
	*offset += *value_len;
	return 0;
}

int tlv_validate_payload(const uint8_t *data, uint16_t data_len,
			 uint8_t field_count)
{
	uint16_t offset = 0;
	uint8_t  actual_count = 0;
	uint8_t  fid, vlen;
	const uint8_t *val;

	while (offset < data_len) {
		if (tlv_decode_field(data, data_len, &offset,
				     &fid, &vlen, &val) != 0) {
			return -1;
		}
		actual_count++;
	}

	/* 检查字段数量是否与声明一致 */
	if (actual_count != field_count) {
		return -1;
	}

	return 0;
}

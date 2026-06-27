/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "app_config.h"
#include "app_fields.h"
#include "app_auth.h"

#include <zephyr/kernel.h>
#include <string.h>
#include <nfc_t4t_lib.h>
#include <nfc/t4t/ndef_file.h>

/* ── 字段数据已迁移至 app_fields.c，此处仅保留传输层逻辑 ── */

/* ── 传感器 setter 函数（转发到字段模块）── */

void app_config_set_battery(uint8_t level)     { app_field_set_battery(level); }
void app_config_set_temperature(int16_t temp)   { app_field_set_temperature(temp); }
void app_config_set_distance(uint16_t dist)     { app_field_set_distance(dist); }
void app_config_set_position(uint8_t pos)       { app_field_set_position(pos); }
void app_config_set_mode(uint8_t mode)          { app_field_set(FIELD_MODE, &mode, 1); }
void app_config_set_update_time(uint16_t interval) {
	uint8_t buf[2];
	buf[0] = (uint8_t)(interval >> 8);
	buf[1] = (uint8_t)(interval);
	app_field_set(FIELD_UPDATE_INTERVAL, buf, 2);
}
void app_config_set_gnss_fix(uint8_t fix)       { app_field_set_gnss_fix(fix); }
void app_config_set_latitude(int32_t lat)       { app_field_set_latitude(lat); }
void app_config_set_longitude(int32_t lng)      { app_field_set_longitude(lng); }
void app_config_set_altitude(int16_t alt)       { app_field_set_altitude(alt); }

/* ── 全局时间戳 ── */

uint32_t app_config_get_unixtime(void)
{
	return app_field_get_unixtime();
}

/* ── 可读名称映射 ── */

static const char *mode_name(uint8_t mode)
{
	switch (mode) {
	case 0x01: return "bin";
	case 0x02: return "trash";
	case 0x03: return "shelf";
	default:   return "unknown";
	}
}

static const char *pos_name(uint8_t pos)
{
	switch (pos) {
	case 0x00: return "normal";
	case 0x01: return "tilt";
	default:   return "unknown";
	}
}

/* ── OTA 状态（调试阶段：数据存 RAM） ── */

struct ota_state {
	bool     active;
	bool     done;
	uint32_t total_size;
	uint16_t next_idx;
	uint32_t bytes_received;
	uint8_t  buf[APP_CONFIG_OTA_BUF_SIZE];
};

static struct ota_state g_ota;

void app_config_ota_get_info(struct app_config_ota_info *info)
{
	info->active           = g_ota.active;
	info->done             = g_ota.done;
	info->total_size       = g_ota.total_size;
	info->packets_received = g_ota.next_idx;
	info->bytes_received   = g_ota.bytes_received;
}

const uint8_t *app_config_ota_get_buf(void)
{
	return g_ota.buf;
}

/* ── NDEF记录标志 ── */

#define NDEF_MB            0x80u
#define NDEF_ME            0x40u
#define NDEF_CF            0x20u
#define NDEF_SR            0x10u
#define NDEF_IL            0x08u
#define NDEF_TNF_MASK      0x07u
#define NDEF_TNF_EXTERNAL  0x04u

/* ── 帧打印 ── */

static void print_frame(const char *channel, const char *dir,
			const uint8_t *buf, size_t len)
{
	printk("[%s] %s (%u bytes):", channel, dir, (unsigned)len);
	for (size_t i = 0; i < len; i++) {
		if (i % 16 == 0) {
			printk("\n  %02x:", (unsigned)i);
		}
		printk(" %02x", buf[i]);
	}
	printk("\n");
}

/* ── 手动NDEF记录解析 ── */

static bool ndef_parse_ext_cmd(const uint8_t *ndef_msg, uint32_t ndef_msg_len,
			       uint8_t *cmd_out,
			       const uint8_t **payload_out,
			       uint32_t *payload_len_out)
{
	if (ndef_msg_len < 3) {
		return false;
	}

	uint8_t flags = ndef_msg[0];
	uint8_t tnf   = flags & NDEF_TNF_MASK;

	if (tnf != NDEF_TNF_EXTERNAL) {
		return false;
	}

	uint8_t type_len = ndef_msg[1];

	uint32_t payload_len;
	uint32_t header_len;

	if (flags & NDEF_SR) {
		payload_len = ndef_msg[2];
		header_len = 3;
	} else {
		if (ndef_msg_len < 6) {
			return false;
		}
		payload_len = ((uint32_t)ndef_msg[2] << 24) |
			      ((uint32_t)ndef_msg[3] << 16) |
			      ((uint32_t)ndef_msg[4] << 8) |
			      ((uint32_t)ndef_msg[5]);
		header_len = 6;
	}

	if (flags & NDEF_IL) {
		if (ndef_msg_len < header_len + 1) {
			return false;
		}
		uint8_t id_len = ndef_msg[header_len];
		header_len += 1 + id_len;
	}

	if (ndef_msg_len < header_len + type_len + payload_len) {
		return false;
	}

	const uint8_t *type = &ndef_msg[header_len];

	if (type_len != APP_CONFIG_CMD_TYPE_LEN) {
		return false;
	}
	if (memcmp(type, APP_CONFIG_CMD_TYPE, APP_CONFIG_CMD_TYPE_LEN) != 0) {
		return false;
	}

	if (payload_len < 1) {
		return false;
	}

	const uint8_t *payload = &ndef_msg[header_len + type_len];
	*cmd_out = payload[0];

	if (payload_out) {
		*payload_out = payload;
	}
	if (payload_len_out) {
		*payload_len_out = payload_len;
	}
	return true;
}

/* ── 手动NDEF消息编码 ── */

/**
 * @brief 将应答payload编码为NDEF消息（"ulpinternal:info" External Type）。
 * @param payload     应答payload（长度可变）
 * @param pay_len     payload 字节数
 * @param dst         目标缓冲区
 * @param dst_len     输入=缓冲区大小，输出=实际编码长度
 * @return 0=成功，负值=错误
 */
static int ndef_encode_response(const uint8_t *payload, uint32_t pay_len,
				uint8_t *dst, uint32_t *dst_len)
{
	uint32_t type_len = APP_CONFIG_INFO_TYPE_LEN;

	/* 记录头: flags(1) + type_len(1) + pay_len_SR(1) = 3 */
	uint32_t total = 3 + type_len + pay_len;

	if (*dst_len < total) {
		return -1;
	}

	uint8_t *p = dst;

	/* flags: MB=1, ME=1, SR=1, TNF=External */
	*p++ = NDEF_MB | NDEF_ME | NDEF_SR | NDEF_TNF_EXTERNAL;
	/* type length */
	*p++ = (uint8_t)type_len;
	/* payload length (SR mode, 1 byte) */
	*p++ = (uint8_t)pay_len;
	/* type */
	memcpy(p, APP_CONFIG_INFO_TYPE, type_len);
	p += type_len;
	/* payload */
	memcpy(p, payload, pay_len);
	p += pay_len;

	*dst_len = (uint32_t)(p - dst);
	return 0;
}

/* ── 公共接口 ── */

bool app_config_handle_ndef(uint8_t *ndef_msg_buf, size_t ndef_msg_buf_size)
{
	/* 获取NDEF消息（跳过2字节NLEN） */
	uint8_t *ndef_msg = nfc_t4t_ndef_file_msg_get(ndef_msg_buf);
	uint32_t ndef_msg_size = nfc_t4t_ndef_file_msg_size_get(ndef_msg_buf_size);

	/* 解析命令 */
	uint8_t cmd;
	const uint8_t *payload;
	uint32_t payload_len;
	if (!ndef_parse_ext_cmd(ndef_msg, ndef_msg_size, &cmd,
				&payload, &payload_len)) {
		return false;
	}

	print_frame("NFC", "RX cmd", payload, payload_len);

	/* ── 统一协议处理 ── */
	uint8_t resp[APP_PROTO_RESP_MAX];
	uint16_t resp_len = process_app_payload("NFC", payload,
						(uint16_t)payload_len,
						resp, sizeof(resp));
	if (resp_len == 0) {
		return false;
	}

	print_frame("NFC", "TX resp", resp, resp_len);

	/* 编码应答NDEF消息 */
	uint8_t *raw_msg = nfc_t4t_ndef_file_msg_get(ndef_msg_buf);
	uint32_t raw_msg_size = nfc_t4t_ndef_file_msg_size_get(ndef_msg_buf_size);

	int err = ndef_encode_response(resp, resp_len, raw_msg, &raw_msg_size);
	if (err) {
		printk("[NFC] encode error %d\n", err);
		return false;
	}

	/* 写入NLEN前缀 */
	err = nfc_t4t_ndef_file_encode(ndef_msg_buf, &raw_msg_size);
	if (err) {
		printk("[NFC] file encode error %d\n", err);
		return false;
	}

	return true;
}

uint16_t app_config_handle_ble(const uint8_t *data, uint16_t len, uint8_t *resp)
{
	if (len < 1) {
		return 0;
	}

	uint8_t cmd = data[0];

	/* ── OTA 命令：保留 BLE 专用逻辑，不进入 process_app_payload ── */

	/* OTA 数据包不打印帧（频繁，会刷屏） */
	if (cmd != APP_CONFIG_CMD_OTA_DATA) {
		print_frame("BLE", "RX cmd", data, len);
	}

	if (cmd == APP_CONFIG_CMD_OTA_START) {
		uint8_t auth_st = app_auth_check_write("BLE");
		if (auth_st != APP_CONFIG_STATUS_OK) {
			resp[0] = APP_CONFIG_CMD_OTA_START;
			resp[1] = auth_st;
			print_frame("BLE", "TX resp", resp, 2);
			return 2;
		}
		memset(&g_ota, 0, offsetof(struct ota_state, buf));
		if (len >= 5) {
			g_ota.total_size = ((uint32_t)data[1] << 24) |
					   ((uint32_t)data[2] << 16) |
					   ((uint32_t)data[3] << 8)  |
					   (uint32_t)data[4];
		}
		g_ota.active = true;
		printk("[BLE] OTA start: total=%u bytes\n", g_ota.total_size);
		resp[0] = APP_CONFIG_CMD_OTA_START;
		resp[1] = APP_CONFIG_STATUS_OK;
		print_frame("BLE", "TX resp", resp, 2);
		return 2;
	}

	if (cmd == APP_CONFIG_CMD_OTA_DATA) {
		uint8_t auth_st = app_auth_check_write("BLE");
		if (auth_st != APP_CONFIG_STATUS_OK) {
			resp[0] = APP_CONFIG_CMD_OTA_DATA;
			resp[1] = auth_st;
			print_frame("BLE", "TX resp", resp, 2);
			return 2;
		}
		if (!g_ota.active) {
			resp[0] = APP_CONFIG_CMD_OTA_DATA;
			resp[1] = APP_CONFIG_STATUS_OTA_STATE;
			printk("[BLE] OTA data without start\n");
			print_frame("BLE", "TX resp", resp, 2);
			return 2;
		}

		if (len >= 4) {
			uint16_t idx      = ((uint16_t)data[1] << 8) | data[2];
			uint16_t data_len = len - 3;
			uint32_t offset   = (uint32_t)idx * APP_CONFIG_OTA_PKT_DATA;

			if (offset + data_len <= APP_CONFIG_OTA_BUF_SIZE) {
				memcpy(&g_ota.buf[offset], &data[3], data_len);
				g_ota.bytes_received += data_len;
			} else {
				printk("[BLE] OTA buf full, drop idx=%u\n", idx);
			}
			g_ota.next_idx = idx + 1;
		}
		return 0;  /* 流式模式不应答 */
	}

	if (cmd == APP_CONFIG_CMD_OTA_END) {
		uint8_t auth_st = app_auth_check_write("BLE");
		if (auth_st != APP_CONFIG_STATUS_OK) {
			resp[0] = APP_CONFIG_CMD_OTA_END;
			resp[1] = auth_st;
			print_frame("BLE", "TX resp", resp, 2);
			return 2;
		}
		g_ota.active = false;
		g_ota.done   = true;
		printk("[BLE] OTA end: %u bytes in %u packets\n",
		       g_ota.bytes_received, g_ota.next_idx);
		resp[0] = APP_CONFIG_CMD_OTA_END;
		resp[1] = APP_CONFIG_STATUS_OK;
		print_frame("BLE", "TX resp", resp, 2);
		return 2;
	}

	/* ── CMD 0x01 / 0x02 / 0x03 / 0x04 统一协议处理 ── */
	uint8_t  internal_buf[APP_PROTO_RESP_MAX];
	uint16_t resp_len = process_app_payload("BLE", data, len,
						internal_buf, sizeof(internal_buf));
	if (resp_len > 0) {
		memcpy(resp, internal_buf, resp_len);
		print_frame("BLE", "TX resp", resp, resp_len);
	}
	return resp_len;
}

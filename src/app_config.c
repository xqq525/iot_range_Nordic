/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "app_config.h"

#include <zephyr/kernel.h>
#include <string.h>
#include <nfc_t4t_lib.h>
#include <nfc/t4t/ndef_file.h>

/* ── 设备固定信息 ── */

static const char g_model[] = "ULP_RS001";
static const char g_sn[]   = "123456";

static const uint8_t g_fw_ver[] = {1, 0, 0};
static const uint8_t g_hw_ver[] = {1, 0, 0};

/* ── 当前传感器数据（由外部更新） ── */

static uint8_t  g_battery     = 100;
static int16_t  g_temperature = 250;   /* 25.0°C, 单位0.1°C */
static uint16_t g_distance    = 15;     /* cm, 0=无读数 */
static uint8_t  g_position    = 0x00;  /* 0=正常, 1=倾斜 */
static uint8_t  g_mode        = 0x02;  /* 1=快递箱, 2=垃圾桶 */
static uint16_t g_update_time = 0;      /* 上报间隔，单位分钟 */

/* ── 供外部模块更新传感器值 ── */

void app_config_set_battery(uint8_t level)     { g_battery = level; }
void app_config_set_temperature(int16_t temp)   { g_temperature = temp; }
void app_config_set_distance(uint16_t dist)     { g_distance = dist; }
void app_config_set_position(uint8_t pos)       { g_position = pos; }
void app_config_set_mode(uint8_t mode)          { g_mode = mode; }
void app_config_set_update_time(uint16_t interval) { g_update_time = interval; }

/* ── NDEF记录标志 ── */

#define NDEF_MB            0x80u
#define NDEF_ME            0x40u
#define NDEF_CF            0x20u
#define NDEF_SR            0x10u
#define NDEF_IL            0x08u
#define NDEF_TNF_MASK      0x07u
#define NDEF_TNF_EXTERNAL  0x04u

/* ── 应答Payload构建 ── */

static void response_build(uint8_t cmd, uint8_t *resp)
{
	memset(resp, 0, APP_CONFIG_RESPONSE_PAYLOAD_SIZE);
	resp[0] = cmd;

	switch (cmd) {
	case APP_CONFIG_CMD_READ_INFO:
	case APP_CONFIG_CMD_WRITE_CONFIG: {
		resp[1] = APP_CONFIG_STATUS_OK;

		/* MODEL: offset 2, 16 bytes */
		size_t ml = strlen(g_model) + 1;
		if (ml > 16) ml = 16;
		memcpy(&resp[2], g_model, ml);

		/* SN: offset 18, 16 bytes */
		size_t sl = strlen(g_sn) + 1;
		if (sl > 16) sl = 16;
		memcpy(&resp[18], g_sn, sl);

		/* FW_VER: offset 34, 3 bytes */
		memcpy(&resp[34], g_fw_ver, 3);

		/* HW_VER: offset 37, 3 bytes */
		memcpy(&resp[37], g_hw_ver, 3);

		/* BATTERY: offset 40 */
		resp[40] = g_battery;

		/* TEMPERATURE: offset 41, int16 BE */
		resp[41] = (uint8_t)(g_temperature >> 8);
		resp[42] = (uint8_t)(g_temperature);

		/* DISTANCE: offset 43, uint16 BE */
		resp[43] = (uint8_t)(g_distance >> 8);
		resp[44] = (uint8_t)(g_distance);

		/* POSITION: offset 45 */
		resp[45] = g_position;

		/* MODE: offset 46 */
		resp[46] = g_mode;

		/* UPDATE_TIME: offset 47, uint16 BE */
		resp[47] = (uint8_t)(g_update_time >> 8);
		resp[48] = (uint8_t)(g_update_time);

		/* RESERVED: offset 49, 1 byte — 已为0 */
		break;
	}
	default:
		resp[1] = APP_CONFIG_STATUS_UNSUPPORTED;
		break;
	}
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
 * @brief 将 ulpinternal:info 应答记录编码为NDEF消息，写入 dst。
 * @param payload     50字节应答payload
 * @param dst         目标缓冲区
 * @param dst_len     输入=缓冲区大小，输出=实际编码长度
 * @return 0=成功，负值=错误
 */
static int ndef_encode_info_response(const uint8_t *payload,
				     uint8_t *dst, uint32_t *dst_len)
{
	uint32_t type_len = APP_CONFIG_INFO_TYPE_LEN;
	uint32_t pay_len  = APP_CONFIG_RESPONSE_PAYLOAD_SIZE;

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

	printk("app_config: received cmd 0x%02x\n", cmd);

	/* 处理 CMD 0x02: 写入配置 */
	if (cmd == APP_CONFIG_CMD_WRITE_CONFIG) {
		if (payload_len < 3) {
			printk("app_config: cmd 0x02 payload too short\n");
			return false;
		}
		uint8_t param_id = payload[1];
		switch (param_id) {
		case APP_CONFIG_PARAM_MODE:
			g_mode = payload[2];
			printk("app_config: mode set to 0x%02x\n", g_mode);
			break;
		case APP_CONFIG_PARAM_REPORT_INTERVAL:
			if (payload_len < 4) {
				printk("app_config: cmd 0x02 interval payload too short\n");
				return false;
			}
			g_update_time = ((uint16_t)payload[2] << 8) | payload[3];
			printk("app_config: update_time set to %u min\n", g_update_time);
			break;
		default:
			printk("app_config: unknown param_id 0x%02x\n", param_id);
			break;
		}
	}

	/* 构建应答payload */
	uint8_t resp[APP_CONFIG_RESPONSE_PAYLOAD_SIZE];
	response_build(cmd, resp);

	/* 编码应答NDEF消息 */
	uint8_t *raw_msg = nfc_t4t_ndef_file_msg_get(ndef_msg_buf);
	uint32_t raw_msg_size = nfc_t4t_ndef_file_msg_size_get(ndef_msg_buf_size);

	int err = ndef_encode_info_response(resp, raw_msg, &raw_msg_size);
	if (err) {
		printk("app_config: encode error %d\n", err);
		return false;
	}

	/* 写入NLEN前缀 */
	err = nfc_t4t_ndef_file_encode(ndef_msg_buf, &raw_msg_size);
	if (err) {
		printk("app_config: file encode error %d\n", err);
		return false;
	}

	/* 注意：不需要调用 nfc_t4t_ndef_rwpayload_set，
	 * 因为 raw_msg 直接修改了已注册的 ndef_msg_buf 内容，
	 * T4T库在下次READ时会自动读取更新后的缓冲区。
	 */

	printk("app_config: response ready for cmd 0x%02x\n", cmd);
	return true;
}

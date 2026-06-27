/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file app_protocol.c
 * @brief V2.0 统一协议处理入口。
 *
 * process_app_payload() 是 NFC 和 BLE 共用的业务 Payload 处理器。
 * 负责 CMD 0x01/0x02/0x03/0x04 的 dispatch。
 * OTA（0x10/0x11/0x12）不进入此函数。
 */

#include "app_protocol.h"
#include "app_tlv.h"
#include "app_fields.h"
#include "app_auth.h"

#include <string.h>
#include <zephyr/sys/printk.h>

/* ── 单次写入最大缓存字段数 ── */
#define WRITE_CACHE_MAX  16

struct write_cache_entry {
	uint8_t       field_id;
	const uint8_t *value;
	uint8_t       value_len;
};

/* ── 全部可读字段列表（按 FIELD_ID 排序，供"读全部"遍历）── */
static const uint8_t ALL_READABLE_FIELDS[] = {
	FIELD_DEVICE_STATE,        /* 0x01 */
	FIELD_DEVICE_TYPE,         /* 0x02 */
	FIELD_MODEL,               /* 0x03 */
	FIELD_SN,                  /* 0x04 */
	FIELD_FW_VER,              /* 0x05 */
	FIELD_HW_VER,              /* 0x06 */
	FIELD_PROTOCOL_VERSION,    /* 0x07 */
	FIELD_BATTERY,             /* 0x10 */
	FIELD_TEMPERATURE,         /* 0x11 */
	FIELD_DISTANCE,            /* 0x12 */
	FIELD_POSITION,            /* 0x13 */
	FIELD_MODE,                /* 0x20 */
	FIELD_UPDATE_INTERVAL,     /* 0x21 */
	FIELD_INSTALL_LOCATION,    /* 0x22 */
	FIELD_GNSS_FIX,            /* 0x23 */
	FIELD_LATITUDE,            /* 0x24 */
	FIELD_LONGITUDE,           /* 0x25 */
	FIELD_ALTITUDE,            /* 0x26 */
};
#define ALL_READABLE_COUNT (sizeof(ALL_READABLE_FIELDS) / sizeof(ALL_READABLE_FIELDS[0]))

/* ═══════════════════════════════════════════════════════════════════════
 * CMD 0x01 — 读取字段
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief 将单个字段编码追加到响应缓冲区。
 *
 * @return 成功返回写入的 TLV 字节数，0 表示字段不支持，
 *         0xFF 表示缓冲区不足。
 */
static uint8_t append_field_to_resp(uint8_t field_id,
				    uint8_t *resp, uint16_t resp_max,
				    uint16_t *pos)
{
	uint8_t val_buf[32];
	uint8_t val_len = app_field_get(field_id, val_buf, sizeof(val_buf));

	if (val_len == 0) {
		return 0; /* 字段不支持 */
	}

	uint8_t tlv_size = 2 + val_len;
	if (*pos + tlv_size > resp_max) {
		return 0xFF; /* 缓冲区不足 */
	}

	*pos += tlv_encode_field(&resp[*pos], field_id, val_buf, val_len);
	return tlv_size;
}

static uint16_t handle_cmd_read(const uint8_t *req, uint16_t req_len,
				uint8_t *resp, uint16_t resp_max)
{
	/* 最小长度: CMD(1) + TS(4) + COUNT(1) = 6 */
	if (req_len < 6) {
		if (resp_max < 3) return 0;
		resp[0] = APP_PROTO_CMD_READ;
		resp[1] = APP_PROTO_STATUS_PARAM_LEN;
		resp[2] = 0x00;
		return 3;
	}

	/* 解析时间戳（大端 uint32） */
	uint32_t ts = ((uint32_t)req[1] << 24) |
		      ((uint32_t)req[2] << 16) |
		      ((uint32_t)req[3] << 8)  |
		      (uint32_t)req[4];
	app_field_set_unixtime(ts);

	uint8_t field_count = req[5];
	const uint8_t *field_list = &req[6];
	uint8_t field_list_len = (uint8_t)(req_len - 6);

	/* 响应头: [CMD, STATUS=0x00 预留, COUNT 预留] */
	if (resp_max < 3) return 0;
	resp[0] = APP_PROTO_CMD_READ;
	resp[1] = APP_PROTO_STATUS_OK;
	/* resp[2] 稍后填入实际字段数 */
	uint16_t pos = 3;
	uint8_t  actual_count = 0;

	if (field_count == 0) {
		/* ── 读全部可读字段 ── */
		for (uint8_t i = 0; i < ALL_READABLE_COUNT; i++) {
			uint8_t ret = append_field_to_resp(
				ALL_READABLE_FIELDS[i], resp, resp_max, &pos);
			if (ret == 0xFF) {
				/* 缓冲区不足，返回错误 */
				resp[0] = APP_PROTO_CMD_READ;
				resp[1] = APP_PROTO_STATUS_UNKNOWN;
				resp[2] = ALL_READABLE_FIELDS[i];
				return 3;
			}
			if (ret > 0) {
				actual_count++;
			}
		}
	} else {
		/* ── 读指定字段 ── */
		if (field_count > field_list_len) {
			resp[0] = APP_PROTO_CMD_READ;
			resp[1] = APP_PROTO_STATUS_PARAM_LEN;
			resp[2] = 0x00;
			return 3;
		}

		/* 先校验全部 FIELD_ID 是否存在且可读 */
		for (uint8_t i = 0; i < field_count; i++) {
			uint8_t fid = field_list[i];
			uint8_t val_buf[32];
			if (app_field_get(fid, val_buf, sizeof(val_buf)) == 0) {
				/* 字段不支持 */
				resp[0] = APP_PROTO_CMD_READ;
				resp[1] = APP_PROTO_STATUS_UNSUPPORTED;
				resp[2] = fid;
				return 3;
			}
		}

		/* 全部合法，编码输出 */
		for (uint8_t i = 0; i < field_count; i++) {
			uint8_t ret = append_field_to_resp(
				field_list[i], resp, resp_max, &pos);
			if (ret == 0xFF) {
				resp[0] = APP_PROTO_CMD_READ;
				resp[1] = APP_PROTO_STATUS_UNKNOWN;
				resp[2] = field_list[i];
				return 3;
			}
			actual_count++;
		}
	}

	resp[2] = actual_count;
	return pos;
}

/* ═══════════════════════════════════════════════════════════════════════
 * CMD 0x02 — 写入配置字段（原子执行）
 * ═══════════════════════════════════════════════════════════════════════ */

static uint16_t handle_cmd_write(const char *channel,
				 const uint8_t *req, uint16_t req_len,
				 uint8_t *resp, uint16_t resp_max)
{
	/* 最小长度: CMD(1) + COUNT(1) = 2 */
	if (req_len < 2) {
		if (resp_max < 3) return 0;
		resp[0] = APP_PROTO_CMD_WRITE;
		resp[1] = APP_PROTO_STATUS_PARAM_LEN;
		resp[2] = 0x00;
		return 3;
	}

	uint8_t field_count = req[1];

	if (field_count == 0) {
		if (resp_max < 3) return 0;
		resp[0] = APP_PROTO_CMD_WRITE;
		resp[1] = APP_PROTO_STATUS_PARAM_LEN;
		resp[2] = 0x00;
		return 3;
	}

	/* ── Phase 1: TLV 结构校验 ── */
	const uint8_t *tlv_data = &req[2];
	uint16_t tlv_len = req_len - 2;

	if (tlv_validate_payload(tlv_data, tlv_len, field_count) != 0) {
		if (resp_max < 3) return 0;
		resp[0] = APP_PROTO_CMD_WRITE;
		resp[1] = APP_PROTO_STATUS_PARAM_LEN;
		resp[2] = 0x00;
		return 3;
	}

	/* ── Phase 2: 逐字段合法性校验（不修改全局变量）── */
	struct write_cache_entry cache[WRITE_CACHE_MAX];
	uint16_t offset = 0;
	uint8_t  cache_count = 0;

	for (uint8_t i = 0; i < field_count; i++) {
		if (cache_count >= WRITE_CACHE_MAX) {
			/* 理论上不应到达（payload 上限 240 字节限制） */
			if (resp_max < 3) return 0;
			resp[0] = APP_PROTO_CMD_WRITE;
			resp[1] = APP_PROTO_STATUS_PARAM_LEN;
			resp[2] = 0x00;
			return 3;
		}

		uint8_t fid, vlen;
		const uint8_t *val;
		if (tlv_decode_field(tlv_data, tlv_len, &offset,
				     &fid, &vlen, &val) != 0) {
			if (resp_max < 3) return 0;
			resp[0] = APP_PROTO_CMD_WRITE;
			resp[1] = APP_PROTO_STATUS_PARAM_LEN;
			resp[2] = 0x00;
			return 3;
		}

		uint8_t status = app_field_validate(fid, val, vlen);
		if (status != APP_PROTO_STATUS_OK) {
			if (resp_max < 3) return 0;
			resp[0] = APP_PROTO_CMD_WRITE;
			resp[1] = status;
			resp[2] = fid;
			printk("[PROTO] %s CMD 0x02 field 0x%02x validate fail: 0x%02x\n",
			       channel, fid, status);
			return 3;
		}

		cache[cache_count].field_id  = fid;
		cache[cache_count].value     = val;
		cache[cache_count].value_len = vlen;
		cache_count++;
	}

	/* ── Phase 3: 权限检查 ── */
	if (!(app_field_get_device_state() == DEVICE_STATE_FACTORY &&
	      !app_field_is_factory_init_done())) {
		uint8_t auth_st = app_auth_check_write(channel);
		if (auth_st != APP_PROTO_STATUS_OK) {
			if (resp_max < 3) return 0;
			resp[0] = APP_PROTO_CMD_WRITE;
			resp[1] = auth_st;
			resp[2] = 0x00;
			printk("[PROTO] %s CMD 0x02 auth required\n", channel);
			return 3;
		}
	}

	/* ── Phase 4: 应用写入 ── */
	for (uint8_t i = 0; i < cache_count; i++) {
		app_field_set(cache[i].field_id,
			      cache[i].value,
			      cache[i].value_len);
	}

	/* ── Phase 5: 状态更新（出厂首次写入后退出出厂状态）── */
	if (app_field_get_device_state() == DEVICE_STATE_FACTORY &&
	    !app_field_is_factory_init_done()) {
		app_field_mark_factory_init_done();
	}

	if (resp_max < 3) return 0;
	resp[0] = APP_PROTO_CMD_WRITE;
	resp[1] = APP_PROTO_STATUS_OK;
	resp[2] = 0x00;
	return 3;
}

/* ═══════════════════════════════════════════════════════════════════════
 * CMD 0x04 — 设备动作控制
 * ═══════════════════════════════════════════════════════════════════════ */

static uint16_t handle_cmd_action(const char *channel,
				  const uint8_t *req, uint16_t req_len,
				  uint8_t *resp, uint16_t resp_max)
{
	/* 最小长度: CMD(1) + SUBCMD(1) = 2 */
	if (req_len < 2) {
		if (resp_max < 4) return 0;
		resp[0] = APP_PROTO_CMD_ACTION;
		resp[1] = APP_PROTO_STATUS_PARAM_LEN;
		resp[2] = 0x00;
		resp[3] = 0x00;
		return 4;
	}

	uint8_t subcmd = req[1];

	switch (subcmd) {
	/* ── 0x01: 开机（不需要认证）── */
	case 0x01: {
		app_field_set_device_state(DEVICE_STATE_POWER_ON);
		if (resp_max < 4) return 0;
		resp[0] = APP_PROTO_CMD_ACTION;
		resp[1] = APP_PROTO_STATUS_OK;
		resp[2] = 0x01;
		resp[3] = app_field_get_device_state();
		printk("[PROTO] %s power on\n", channel);
		return 4;
	}

	/* ── 0x02: 关机（需要认证）── */
	case 0x02: {
		uint8_t auth_st = app_auth_check_write(channel);
		if (auth_st != APP_PROTO_STATUS_OK) {
			if (resp_max < 4) return 0;
			resp[0] = APP_PROTO_CMD_ACTION;
			resp[1] = auth_st;
			resp[2] = 0x02;
			resp[3] = app_field_get_device_state();
			return 4;
		}
		app_field_set_device_state(DEVICE_STATE_POWER_OFF);
		if (resp_max < 4) return 0;
		resp[0] = APP_PROTO_CMD_ACTION;
		resp[1] = APP_PROTO_STATUS_OK;
		resp[2] = 0x02;
		resp[3] = app_field_get_device_state();
		printk("[PROTO] %s power off\n", channel);
		return 4;
	}

	/* ── 0x03: 恢复出厂（需要认证 + "FACTORY" 二次确认）── */
	case 0x03: {
		uint8_t auth_st = app_auth_check_write(channel);
		if (auth_st != APP_PROTO_STATUS_OK) {
			if (resp_max < 4) return 0;
			resp[0] = APP_PROTO_CMD_ACTION;
			resp[1] = auth_st;
			resp[2] = 0x03;
			resp[3] = app_field_get_device_state();
			return 4;
		}

		/* 检查二次确认字符串 "FACTORY"（7 字节 ASCII） */
		if (req_len < 9 ||
		    memcmp(&req[2], "FACTORY", 7) != 0) {
			if (resp_max < 4) return 0;
			resp[0] = APP_PROTO_CMD_ACTION;
			resp[1] = APP_PROTO_STATUS_PARAM_VALUE;
			resp[2] = 0x03;
			resp[3] = app_field_get_device_state();
			return 4;
		}

		/* 恢复出厂：重新初始化全部字段、状态和密码 */
		app_fields_init();
		app_auth_reset_password();
		if (resp_max < 4) return 0;
		resp[0] = APP_PROTO_CMD_ACTION;
		resp[1] = APP_PROTO_STATUS_OK;
		resp[2] = 0x03;
		resp[3] = DEVICE_STATE_FACTORY;
		printk("[PROTO] %s factory reset\n", channel);
		return 4;
	}

	default:
		if (resp_max < 3) return 0;
		resp[0] = APP_PROTO_CMD_ACTION;
		resp[1] = APP_PROTO_STATUS_UNSUPPORTED;
		resp[2] = subcmd;
		return 3;
	}
}

/* ═══════════════════════════════════════════════════════════════════════
 * 统一入口
 * ═══════════════════════════════════════════════════════════════════════ */

uint16_t process_app_payload(const char *channel,
			     const uint8_t *req, uint16_t req_len,
			     uint8_t *resp, uint16_t resp_max)
{
	if (req_len < 1) {
		return 0;
	}

	uint8_t cmd = req[0];

	switch (cmd) {
	case APP_PROTO_CMD_READ:
		return handle_cmd_read(req, req_len, resp, resp_max);

	case APP_PROTO_CMD_WRITE:
		return handle_cmd_write(channel, req, req_len, resp, resp_max);

	case APP_PROTO_CMD_AUTH:
		/* 直接调用现有认证模块，签名不变 */
		return app_auth_handle_cmd(req, req_len, resp, channel);

	case APP_PROTO_CMD_ACTION:
		return handle_cmd_action(channel, req, req_len, resp, resp_max);

	default:
		if (resp_max < 2) return 0;
		resp[0] = cmd;
		resp[1] = APP_PROTO_STATUS_UNSUPPORTED;
		return 2;
	}
}

/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/shell/shell.h>
#include <stdlib.h>
#include "app_config.h"
#include "app_tlv.h"
#include "app_protocol.h"

/* ── TLV 解析辅助：从响应中按 FIELD_ID 查找字段值 ── */
static const uint8_t *tlv_find_field(const uint8_t *resp, uint16_t resp_len,
				     uint8_t target_fid, uint8_t *out_len)
{
	/* 响应格式: [CMD(1), STATUS(1), COUNT(1), TLV...] */
	if (resp_len < 3) return NULL;

	uint8_t count = resp[2];
	uint16_t off = 3;

	for (uint8_t i = 0; i < count; i++) {
		uint8_t fid, vlen;
		const uint8_t *val;
		if (tlv_decode_field(resp, resp_len, &off, &fid, &vlen, &val) != 0) {
			return NULL;
		}
		if (fid == target_fid) {
			*out_len = vlen;
			return val;
		}
	}
	return NULL;
}

static int16_t tlv_get_s16(const uint8_t *val)
{
	return (int16_t)(((uint16_t)val[0] << 8) | val[1]);
}

static uint16_t tlv_get_u16(const uint8_t *val)
{
	return ((uint16_t)val[0] << 8) | val[1];
}

static int32_t tlv_get_s32(const uint8_t *val)
{
	return ((int32_t)val[0] << 24) | ((int32_t)val[1] << 16) |
	       ((int32_t)val[2] << 8)  |  (int32_t)val[3];
}

/* config get — V2.0 TLV 解析 */
static int cmd_config_get(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	/* 构造 CMD 0x01 读全部: [CMD=0x01, TS(4)=0, COUNT(1)=0] */
	uint8_t req[6];
	req[0] = APP_PROTO_CMD_READ;
	req[1] = 0; req[2] = 0; req[3] = 0; req[4] = 0; /* unixtime 占位 */
	req[5] = 0; /* count=0 读全部 */

	uint8_t resp[APP_PROTO_RESP_MAX];
	uint16_t resp_len = app_config_handle_ble(req, sizeof(req), resp);

	if (resp_len < 3) {
		shell_error(sh, "no/invalid response (len=%u)", resp_len);
		return -EIO;
	}
	if (resp[1] != APP_PROTO_STATUS_OK) {
		shell_error(sh, "error status 0x%02x", resp[1]);
		return -EIO;
	}

	uint8_t vlen;
	const uint8_t *v;

	/* 字符串字段 */
	v = tlv_find_field(resp, resp_len, FIELD_MODEL, &vlen);
	if (v) shell_print(sh, "model       : %.*s", vlen, v);

	v = tlv_find_field(resp, resp_len, FIELD_SN, &vlen);
	if (v) shell_print(sh, "sn          : %.*s", vlen, v);

	/* 版本字段 */
	v = tlv_find_field(resp, resp_len, FIELD_FW_VER, &vlen);
	if (v && vlen >= 3) shell_print(sh, "fw          : %d.%d.%d", v[0], v[1], v[2]);

	v = tlv_find_field(resp, resp_len, FIELD_HW_VER, &vlen);
	if (v && vlen >= 3) shell_print(sh, "hw          : %d.%d.%d", v[0], v[1], v[2]);

	/* 状态字段 */
	v = tlv_find_field(resp, resp_len, FIELD_DEVICE_STATE, &vlen);
	if (v) shell_print(sh, "state       : 0x%02x", v[0]);

	v = tlv_find_field(resp, resp_len, FIELD_BATTERY, &vlen);
	if (v) shell_print(sh, "battery     : %d%%", v[0]);

	v = tlv_find_field(resp, resp_len, FIELD_TEMPERATURE, &vlen);
	if (v) {
		int16_t t = tlv_get_s16(v);
		shell_print(sh, "temperature : %d.%d C", t / 10, abs(t % 10));
	}

	v = tlv_find_field(resp, resp_len, FIELD_DISTANCE, &vlen);
	if (v) shell_print(sh, "distance    : %d cm", tlv_get_u16(v));

	v = tlv_find_field(resp, resp_len, FIELD_POSITION, &vlen);
	if (v) shell_print(sh, "position    : 0x%02x", v[0]);

	v = tlv_find_field(resp, resp_len, FIELD_MODE, &vlen);
	if (v) shell_print(sh, "mode        : 0x%02x", v[0]);

	v = tlv_find_field(resp, resp_len, FIELD_UPDATE_INTERVAL, &vlen);
	if (v) shell_print(sh, "interval    : %d min", tlv_get_u16(v));

	v = tlv_find_field(resp, resp_len, FIELD_PROTOCOL_VERSION, &vlen);
	if (v) shell_print(sh, "protocol    : 0x%02x", v[0]);

	/* GNSS 字段 */
	v = tlv_find_field(resp, resp_len, FIELD_GNSS_FIX, &vlen);
	if (v && v[0]) {
		shell_print(sh, "gnss_fix    : %d", v[0]);

		v = tlv_find_field(resp, resp_len, FIELD_LATITUDE, &vlen);
		if (v) shell_print(sh, "latitude    : %d (x1e7)", tlv_get_s32(v));

		v = tlv_find_field(resp, resp_len, FIELD_LONGITUDE, &vlen);
		if (v) shell_print(sh, "longitude   : %d (x1e7)", tlv_get_s32(v));

		v = tlv_find_field(resp, resp_len, FIELD_ALTITUDE, &vlen);
		if (v) shell_print(sh, "altitude    : %d m", (int16_t)tlv_get_u16(v));
	} else {
		shell_print(sh, "gnss_fix    : 0 (no fix)");
	}

	return 0;
}

static int cmd_config_set_battery(const struct shell *sh, size_t argc, char **argv)
{
	uint8_t val = (uint8_t)strtoul(argv[1], NULL, 0);

	app_config_set_battery(val);
	shell_print(sh, "battery set to %d%%", val);
	return 0;
}

static int cmd_config_set_temperature(const struct shell *sh, size_t argc, char **argv)
{
	int16_t val = (int16_t)strtol(argv[1], NULL, 0);

	app_config_set_temperature(val);
	shell_print(sh, "temperature set to %d.%d C", val / 10, abs(val % 10));
	return 0;
}

static int cmd_config_set_distance(const struct shell *sh, size_t argc, char **argv)
{
	uint16_t val = (uint16_t)strtoul(argv[1], NULL, 0);

	app_config_set_distance(val);
	shell_print(sh, "distance set to %d cm", val);
	return 0;
}

static int cmd_config_set_position(const struct shell *sh, size_t argc, char **argv)
{
	uint8_t val = (uint8_t)strtoul(argv[1], NULL, 0);

	app_config_set_position(val);
	shell_print(sh, "position set to 0x%02x", val);
	return 0;
}

static int cmd_config_set_mode(const struct shell *sh, size_t argc, char **argv)
{
	uint8_t val = (uint8_t)strtoul(argv[1], NULL, 0);

	app_config_set_mode(val);
	shell_print(sh, "mode set to 0x%02x", val);
	return 0;
}

static int cmd_config_set_interval(const struct shell *sh, size_t argc, char **argv)
{
	uint16_t val = (uint16_t)strtoul(argv[1], NULL, 0);

	app_config_set_update_time(val);
	shell_print(sh, "interval set to %d min", val);
	return 0;
}

static int cmd_config_set_gnss_fix(const struct shell *sh, size_t argc, char **argv)
{
	uint8_t val = (uint8_t)strtoul(argv[1], NULL, 0);

	app_config_set_gnss_fix(val);
	shell_print(sh, "gnss_fix set to %d", val);
	return 0;
}

static int cmd_config_set_latitude(const struct shell *sh, size_t argc, char **argv)
{
	int32_t val = (int32_t)strtol(argv[1], NULL, 0);

	app_config_set_latitude(val);
	shell_print(sh, "latitude set to %d (x1e7)", val);
	return 0;
}

static int cmd_config_set_longitude(const struct shell *sh, size_t argc, char **argv)
{
	int32_t val = (int32_t)strtol(argv[1], NULL, 0);

	app_config_set_longitude(val);
	shell_print(sh, "longitude set to %d (x1e7)", val);
	return 0;
}

static int cmd_config_set_altitude(const struct shell *sh, size_t argc, char **argv)
{
	int16_t val = (int16_t)strtol(argv[1], NULL, 0);

	app_config_set_altitude(val);
	shell_print(sh, "altitude set to %d m", val);
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(config_set_cmds,
	SHELL_CMD_ARG(battery,     NULL, "Set battery level <0-100>",        cmd_config_set_battery,     2, 0),
	SHELL_CMD_ARG(temperature, NULL, "Set temperature <0.1C, e.g. 250>", cmd_config_set_temperature, 2, 0),
	SHELL_CMD_ARG(distance,    NULL, "Set distance <cm>",                 cmd_config_set_distance,    2, 0),
	SHELL_CMD_ARG(position,    NULL, "Set position <val>",                cmd_config_set_position,    2, 0),
	SHELL_CMD_ARG(mode,        NULL, "Set mode <val>",                    cmd_config_set_mode,        2, 0),
	SHELL_CMD_ARG(interval,    NULL, "Set report interval <min>",         cmd_config_set_interval,    2, 0),
	SHELL_CMD_ARG(gnss_fix,    NULL, "Set GNSS fix status <0-1>",         cmd_config_set_gnss_fix,    2, 0),
	SHELL_CMD_ARG(latitude,    NULL, "Set latitude <val*1e7>",             cmd_config_set_latitude,    2, 0),
	SHELL_CMD_ARG(longitude,   NULL, "Set longitude <val*1e7>",            cmd_config_set_longitude,   2, 0),
	SHELL_CMD_ARG(altitude,    NULL, "Set altitude <m>",                   cmd_config_set_altitude,    2, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(config_cmds,
	SHELL_CMD(get, NULL,             "Read device info", cmd_config_get),
	SHELL_CMD(set, &config_set_cmds, "Write config",     NULL),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(config, &config_cmds, "Device configuration", NULL);

/* ── OTA 调试命令 ── */

static int cmd_ota_info(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	struct app_config_ota_info info;

	app_config_ota_get_info(&info);

	const char *state = info.done ? "done" : (info.active ? "receiving" : "idle");

	shell_print(sh, "state         : %s", state);
	shell_print(sh, "total_size    : %u bytes", info.total_size);
	shell_print(sh, "packets_rcvd  : %u", info.packets_received);
	shell_print(sh, "bytes_rcvd    : %u", info.bytes_received);
	return 0;
}

static int cmd_ota_dump(const struct shell *sh, size_t argc, char **argv)
{
	uint32_t offset = (uint32_t)strtoul(argv[1], NULL, 0);
	uint32_t len    = (uint32_t)strtoul(argv[2], NULL, 0);

	if (len > 512) {
		len = 512;
	}
	if (offset >= APP_CONFIG_OTA_BUF_SIZE) {
		shell_error(sh, "offset out of range");
		return -EINVAL;
	}
	if (offset + len > APP_CONFIG_OTA_BUF_SIZE) {
		len = APP_CONFIG_OTA_BUF_SIZE - offset;
	}

	const uint8_t *buf = app_config_ota_get_buf();
	char line[58];  /* "xxxx: " + 16×" xx" = 5 + 48 = 53 */

	for (uint32_t i = 0; i < len; i += 16) {
		int pos = snprintf(line, sizeof(line), "%04x:", offset + i);

		for (uint32_t j = i; j < len && j < i + 16; j++) {
			pos += snprintf(line + pos, sizeof(line) - pos,
					" %02x", buf[offset + j]);
		}
		shell_print(sh, "%s", line);
	}
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(ota_cmds,
	SHELL_CMD(info, NULL, "OTA receive status", cmd_ota_info),
	SHELL_CMD_ARG(dump, NULL, "Hex dump OTA buffer: dump <offset> <len>",
		      cmd_ota_dump, 3, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(ota, &ota_cmds, "OTA firmware debug", NULL);

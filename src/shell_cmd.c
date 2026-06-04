/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/shell/shell.h>
#include <stdlib.h>
#include "app_config.h"

/* config get */
static int cmd_config_get(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	uint8_t req = APP_CONFIG_CMD_READ_INFO;
	uint8_t resp[APP_CONFIG_RESPONSE_PAYLOAD_SIZE];

	app_config_handle_ble(&req, 1, resp);

	if (resp[1] != APP_CONFIG_STATUS_OK) {
		shell_error(sh, "error status 0x%02x", resp[1]);
		return -EIO;
	}

	int16_t temp_raw = ((int16_t)resp[44] << 8) | resp[45];
	uint16_t dist    = ((uint16_t)resp[46] << 8) | resp[47];
	uint16_t intv    = ((uint16_t)resp[50] << 8) | resp[51];

	shell_print(sh, "model       : %s",    (char *)&resp[5]);
	shell_print(sh, "sn          : %s",    (char *)&resp[21]);
	shell_print(sh, "fw          : %d.%d.%d", resp[37], resp[38], resp[39]);
	shell_print(sh, "hw          : %d.%d.%d", resp[40], resp[41], resp[42]);
	shell_print(sh, "battery     : %d%%",  resp[43]);
	shell_print(sh, "temperature : %d.%d C", temp_raw / 10, abs(temp_raw % 10));
	shell_print(sh, "distance    : %d cm", dist);
	shell_print(sh, "position    : 0x%02x", resp[48]);
	shell_print(sh, "mode        : 0x%02x", resp[49]);
	shell_print(sh, "interval    : %d min", intv);

	if (resp[52]) {
		int32_t lat = ((int32_t)resp[53] << 24) |
			      ((int32_t)resp[54] << 16) |
			      ((int32_t)resp[55] << 8)  |
			      (int32_t)resp[56];
		int32_t lng = ((int32_t)resp[57] << 24) |
			      ((int32_t)resp[58] << 16) |
			      ((int32_t)resp[59] << 8)  |
			      (int32_t)resp[60];
		int16_t alt = ((int16_t)resp[61] << 8) | resp[62];
		shell_print(sh, "gnss_fix    : %d",     resp[52]);
		shell_print(sh, "latitude    : %d (x1e7)", lat);
		shell_print(sh, "longitude   : %d (x1e7)", lng);
		shell_print(sh, "altitude    : %d m",      alt);
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

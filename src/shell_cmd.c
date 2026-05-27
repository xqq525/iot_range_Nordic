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

	int16_t temp_raw = ((int16_t)resp[41] << 8) | resp[42];
	uint16_t dist    = ((uint16_t)resp[43] << 8) | resp[44];
	uint16_t intv    = ((uint16_t)resp[47] << 8) | resp[48];

	shell_print(sh, "model       : %s",    (char *)&resp[2]);
	shell_print(sh, "sn          : %s",    (char *)&resp[18]);
	shell_print(sh, "fw          : %d.%d.%d", resp[34], resp[35], resp[36]);
	shell_print(sh, "hw          : %d.%d.%d", resp[37], resp[38], resp[39]);
	shell_print(sh, "battery     : %d%%",  resp[40]);
	shell_print(sh, "temperature : %d.%d C", temp_raw / 10, abs(temp_raw % 10));
	shell_print(sh, "distance    : %d cm", dist);
	shell_print(sh, "position    : 0x%02x", resp[45]);
	shell_print(sh, "mode        : 0x%02x", resp[46]);
	shell_print(sh, "interval    : %d min", intv);

	return 0;
}

/* config set mode <val> */
static int cmd_config_set_mode(const struct shell *sh, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(sh, "usage: config set mode <0-255>");
		return -EINVAL;
	}

	uint8_t mode = (uint8_t)strtoul(argv[1], NULL, 0);
	uint8_t cmd[] = {APP_CONFIG_CMD_WRITE_CONFIG, APP_CONFIG_PARAM_MODE, mode};
	uint8_t resp[APP_CONFIG_RESPONSE_PAYLOAD_SIZE];

	app_config_handle_ble(cmd, sizeof(cmd), resp);

	shell_print(sh, "mode set to 0x%02x", mode);
	return 0;
}

/* config set interval <minutes> */
static int cmd_config_set_interval(const struct shell *sh, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(sh, "usage: config set interval <minutes>");
		return -EINVAL;
	}

	uint16_t interval = (uint16_t)strtoul(argv[1], NULL, 0);
	uint8_t cmd[] = {
		APP_CONFIG_CMD_WRITE_CONFIG,
		APP_CONFIG_PARAM_REPORT_INTERVAL,
		(uint8_t)(interval >> 8),
		(uint8_t)(interval),
	};
	uint8_t resp[APP_CONFIG_RESPONSE_PAYLOAD_SIZE];

	app_config_handle_ble(cmd, sizeof(cmd), resp);

	shell_print(sh, "interval set to %d min", interval);
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(config_set_cmds,
	SHELL_CMD_ARG(mode,     NULL, "Set mode <val>",           cmd_config_set_mode,     2, 0),
	SHELL_CMD_ARG(interval, NULL, "Set report interval <min>", cmd_config_set_interval, 2, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(config_cmds,
	SHELL_CMD(get, NULL,          "Read device info",  cmd_config_get),
	SHELL_CMD(set, &config_set_cmds, "Write config",   NULL),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(config, &config_cmds, "Device configuration", NULL);

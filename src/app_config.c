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

/* ── GNSS 定位数据 ── */

static uint8_t  g_gnss_fix    = 0x01;
static int32_t  g_latitude    = 246087800;   /* 24.60878°N × 10^7 */
static int32_t  g_longitude   = 1180689300;  /* 118.06893°E × 10^7 */
static int16_t  g_altitude    = 15;           /* 15m */

/* ── 供外部模块更新传感器值 ── */

void app_config_set_battery(uint8_t level)     { g_battery = level; }
void app_config_set_temperature(int16_t temp)   { g_temperature = temp; }
void app_config_set_distance(uint16_t dist)     { g_distance = dist; }
void app_config_set_position(uint8_t pos)       { g_position = pos; }
void app_config_set_mode(uint8_t mode)          { g_mode = mode; }
void app_config_set_update_time(uint16_t interval) { g_update_time = interval; }
void app_config_set_gnss_fix(uint8_t fix)       { g_gnss_fix = fix; }
void app_config_set_latitude(int32_t lat)       { g_latitude = lat; }
void app_config_set_longitude(int32_t lng)      { g_longitude = lng; }
void app_config_set_altitude(int16_t alt)       { g_altitude = alt; }

/* ── 全局时间戳 ── */

static uint32_t g_unixtime;  /* APP 最后一次 CMD 0x01 携带的时间戳，秒级 */

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

/* ── 打印全部配置 ── */

static void print_config(void)
{
	printk("  model       : %s\n", g_model);
	printk("  sn          : %s\n", g_sn);
	printk("  fw          : %d.%d.%d\n", g_fw_ver[0], g_fw_ver[1], g_fw_ver[2]);
	printk("  hw          : %d.%d.%d\n", g_hw_ver[0], g_hw_ver[1], g_hw_ver[2]);
	printk("  battery     : %u%%\n", g_battery);
	printk("  temperature : %d.%d C\n", g_temperature / 10, (g_temperature % 10 + 10) % 10);
	printk("  distance    : %u cm\n", g_distance);
	printk("  position    : 0x%02x (%s)\n", g_position, pos_name(g_position));
	printk("  mode        : 0x%02x (%s)\n", g_mode, mode_name(g_mode));
	printk("  interval    : %u min\n", g_update_time);
	printk("  gnss_fix    : %u\n", g_gnss_fix);
	printk("  latitude    : %d (x1e7)\n", g_latitude);
	printk("  longitude   : %d (x1e7)\n", g_longitude);
	printk("  altitude    : %d m\n", g_altitude);
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

uint32_t app_config_get_unixtime(void) { return g_unixtime; }

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

		/* DATA_LEN: offset 2, uint16 BE — offset 4起共48字节 */
		resp[2] = (uint8_t)((APP_CONFIG_RESPONSE_PAYLOAD_SIZE - 4) >> 8);
		resp[3] = (uint8_t)(APP_CONFIG_RESPONSE_PAYLOAD_SIZE - 4);

		/* DEVICE_TYPE: offset 4 */
		resp[4] = APP_CONFIG_DEVICE_TYPE;

		/* MODEL: offset 5, 16 bytes */
		size_t ml = strlen(g_model) + 1;
		if (ml > 16) ml = 16;
		memcpy(&resp[5], g_model, ml);

		/* SN: offset 21, 16 bytes */
		size_t sl = strlen(g_sn) + 1;
		if (sl > 16) sl = 16;
		memcpy(&resp[21], g_sn, sl);

		/* FW_VER: offset 37, 3 bytes */
		memcpy(&resp[37], g_fw_ver, 3);

		/* HW_VER: offset 40, 3 bytes */
		memcpy(&resp[40], g_hw_ver, 3);

		/* BATTERY: offset 43 */
		resp[43] = g_battery;

		/* TEMPERATURE: offset 44, int16 BE */
		resp[44] = (uint8_t)(g_temperature >> 8);
		resp[45] = (uint8_t)(g_temperature);

		/* DISTANCE: offset 46, uint16 BE */
		resp[46] = (uint8_t)(g_distance >> 8);
		resp[47] = (uint8_t)(g_distance);

		/* POSITION: offset 48 */
		resp[48] = g_position;

		/* MODE: offset 49 */
		resp[49] = g_mode;

		/* UPDATE_TIME: offset 50, uint16 BE */
		resp[50] = (uint8_t)(g_update_time >> 8);
		resp[51] = (uint8_t)(g_update_time);

		/* GNSS_FIX: offset 52 */
		resp[52] = g_gnss_fix;

		/* LATITUDE: offset 53, int32 BE */
		resp[53] = (uint8_t)(g_latitude >> 24);
		resp[54] = (uint8_t)(g_latitude >> 16);
		resp[55] = (uint8_t)(g_latitude >> 8);
		resp[56] = (uint8_t)(g_latitude);

		/* LONGITUDE: offset 57, int32 BE */
		resp[57] = (uint8_t)(g_longitude >> 24);
		resp[58] = (uint8_t)(g_longitude >> 16);
		resp[59] = (uint8_t)(g_longitude >> 8);
		resp[60] = (uint8_t)(g_longitude);

		/* ALTITUDE: offset 61, int16 BE */
		resp[61] = (uint8_t)(g_altitude >> 8);
		resp[62] = (uint8_t)(g_altitude);

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

/* ── 命令参数处理（NFC/BLE共用） ── */

static uint8_t apply_write_config(const uint8_t *args, uint32_t args_len,
				   const char *channel)
{
	if (args_len < 2) {
		printk("[%s] CMD 0x02 payload too short\n", channel);
		return APP_CONFIG_STATUS_OK;
	}
	uint8_t param_id = args[0];
	switch (param_id) {
	case APP_CONFIG_PARAM_MODE: {
		uint8_t old = g_mode;
		g_mode = args[1];
		printk("[%s] mode: %s (0x%02x) -> %s (0x%02x)\n",
		       channel, mode_name(old), old, mode_name(g_mode), g_mode);
		return APP_CONFIG_STATUS_OK;
	}
	case APP_CONFIG_PARAM_REPORT_INTERVAL:
		if (args_len < 3) {
			printk("[%s] interval payload too short\n", channel);
			return APP_CONFIG_STATUS_OK;
		}
		{
			uint16_t old = g_update_time;
			g_update_time = ((uint16_t)args[1] << 8) | args[2];
			printk("[%s] interval: %u min -> %u min\n",
			       channel, old, g_update_time);
		}
		return APP_CONFIG_STATUS_OK;
	case APP_CONFIG_PARAM_INSTALL_POS:
		if (args_len < 9) {
			printk("[%s] install_pos payload too short\n", channel);
			return APP_CONFIG_STATUS_OK;
		}
		{
			int32_t old_lat = g_latitude;
			int32_t old_lng = g_longitude;
			g_latitude  = ((int32_t)args[1] << 24) |
				      ((int32_t)args[2] << 16) |
				      ((int32_t)args[3] << 8)  |
				      (int32_t)args[4];
			g_longitude = ((int32_t)args[5] << 24) |
				      ((int32_t)args[6] << 16) |
				      ((int32_t)args[7] << 8)  |
				      (int32_t)args[8];
			g_gnss_fix = 1;
			g_altitude = 0;
			printk("[%s] install_pos: lat %d -> %d, lng %d -> %d\n",
			       channel, old_lat, g_latitude, old_lng, g_longitude);
		}
		return APP_CONFIG_STATUS_OK;
	case APP_CONFIG_PARAM_MODEL_CHECK: {
		/* args[1..] 为 APP 下发的设备型号字符串（含 '\0'） */
		uint32_t model_len = args_len - 1;
		if (model_len > 16) model_len = 16;
		if (model_len == strlen(g_model) + 1 &&
		    memcmp(&args[1], g_model, model_len) == 0) {
			printk("[%s] model check: match (%s)\n", channel, g_model);
			return APP_CONFIG_STATUS_OK;
		}
		printk("[%s] model check: mismatch (expected %s)\n", channel, g_model);
		return APP_CONFIG_STATUS_MODEL_MISMATCH;
	}
	default:
		printk("[%s] unknown param_id 0x%02x\n", channel, param_id);
		return APP_CONFIG_STATUS_OK;
	}
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

	/* CMD 0x01: 解析时间戳 + 打印全部配置 */
	if (cmd == APP_CONFIG_CMD_READ_INFO) {
		if (payload_len >= 5) {
			g_unixtime = ((uint32_t)payload[1] << 24) |
				     ((uint32_t)payload[2] << 16) |
				     ((uint32_t)payload[3] << 8)  |
				     (uint32_t)payload[4];
		}
		printk("[NFC] config get\n");
		print_config();
	}

	/* 处理 CMD 0x02: 写入配置 */
	uint8_t wstatus = APP_CONFIG_STATUS_OK;
	if (cmd == APP_CONFIG_CMD_WRITE_CONFIG) {
		if (payload_len < 3) {
			printk("[NFC] CMD 0x02 payload too short\n");
			return false;
		}
		/* payload[0]=CMD, payload[1]=param_id, payload[2..]=value */
		wstatus = apply_write_config(&payload[1], payload_len - 1, "NFC");
	}

	/* 构建应答payload */
	uint8_t resp[APP_CONFIG_RESPONSE_PAYLOAD_SIZE];
	response_build(cmd, resp);
	if (wstatus != APP_CONFIG_STATUS_OK) {
		resp[1] = wstatus;
	}

	print_frame("NFC", "TX resp", resp, sizeof(resp));

	/* 编码应答NDEF消息 */
	uint8_t *raw_msg = nfc_t4t_ndef_file_msg_get(ndef_msg_buf);
	uint32_t raw_msg_size = nfc_t4t_ndef_file_msg_size_get(ndef_msg_buf_size);

	int err = ndef_encode_info_response(resp, raw_msg, &raw_msg_size);
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

	/* 注意：不需要调用 nfc_t4t_ndef_rwpayload_set，
	 * 因为 raw_msg 直接修改了已注册的 ndef_msg_buf 内容，
	 * T4T库在下次READ时会自动读取更新后的缓冲区。
	 */

	return true;
}

uint16_t app_config_handle_ble(const uint8_t *data, uint16_t len, uint8_t *resp)
{
	if (len < 1) {
		return 0;
	}

	uint8_t cmd = data[0];

	/* OTA 数据包不打印帧（频繁，会刷屏） */
	if (cmd != APP_CONFIG_CMD_OTA_DATA) {
		print_frame("BLE", "RX cmd", data, len);
	}

	/* ── OTA 命令处理 ── */
	if (cmd == APP_CONFIG_CMD_OTA_START) {
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
		g_ota.active = false;
		g_ota.done   = true;
		printk("[BLE] OTA end: %u bytes in %u packets\n",
		       g_ota.bytes_received, g_ota.next_idx);
		resp[0] = APP_CONFIG_CMD_OTA_END;
		resp[1] = APP_CONFIG_STATUS_OK;
		print_frame("BLE", "TX resp", resp, 2);
		return 2;
	}

	/* ── 原有命令 0x01 / 0x02 ── */
	uint8_t wstatus = APP_CONFIG_STATUS_OK;
	if (cmd == APP_CONFIG_CMD_READ_INFO) {
		if (len >= 5) {
			g_unixtime = ((uint32_t)data[1] << 24) |
				     ((uint32_t)data[2] << 16) |
				     ((uint32_t)data[3] << 8)  |
				     (uint32_t)data[4];
		}
		printk("[BLE] config get\n");
		print_config();
	} else if (cmd == APP_CONFIG_CMD_WRITE_CONFIG) {
		if (len < 3) {
			printk("[BLE] CMD 0x02 payload too short\n");
			resp[0] = cmd;
			resp[1] = APP_CONFIG_STATUS_BUSY;
			memset(&resp[2], 0, APP_CONFIG_RESPONSE_PAYLOAD_SIZE - 2);
			print_frame("BLE", "TX resp", resp, APP_CONFIG_RESPONSE_PAYLOAD_SIZE);
			return APP_CONFIG_RESPONSE_PAYLOAD_SIZE;
		}
		wstatus = apply_write_config(&data[1], len - 1, "BLE");
	}

	response_build(cmd, resp);
	if (wstatus != APP_CONFIG_STATUS_OK) {
		resp[1] = wstatus;
	}
	print_frame("BLE", "TX resp", resp, APP_CONFIG_RESPONSE_PAYLOAD_SIZE);
	return APP_CONFIG_RESPONSE_PAYLOAD_SIZE;
}

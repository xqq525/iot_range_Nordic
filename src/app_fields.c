/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file app_fields.c
 * @brief V2.0 字段注册表实现。
 *
 * 管理所有 FIELD_ID 的存储、读写、校验。设备状态 RAM-only。
 * 网络/平台预留字段 0x30~0x36 不注册，返回 UNSUPPORTED。
 */

#include "app_fields.h"
#include "app_protocol.h"
#include <string.h>
#include <zephyr/sys/printk.h>

/* ═══════════════════════════════════════════════════════════════════════
 * 字段数据存储
 * ═══════════════════════════════════════════════════════════════════════ */

/* 设备基础信息（只读） */
static uint8_t  g_device_type     = DEVICE_TYPE_RL;       /* 0x02 */
static char     g_model[]         = "RL_TN100";           /* 0x03 */
static char     g_sn[]            = "123456";             /* 0x04 */
static uint8_t  g_fw_ver[3]       = {1, 0, 0};           /* 0x05 */
static uint8_t  g_hw_ver[3]       = {1, 0, 0};           /* 0x06 */
static uint8_t  g_protocol_ver    = PROTOCOL_VERSION_VALUE; /* 0x07 */

/* 传感器数据（只读） */
static uint8_t  g_battery         = 100;                  /* 0x10 */
static int16_t  g_temperature     = 250;                  /* 0x11, 25.0°C */
static uint16_t g_distance        = 15;                   /* 0x12, cm */
static uint8_t  g_position        = 0x00;                 /* 0x13 */

/* 可写配置字段 */
static uint8_t  g_mode            = 0x02;                 /* 0x20, 1=快递箱 2=垃圾桶 3=货架 */
static uint16_t g_update_interval = 0;                    /* 0x21, 分钟 */
static int32_t  g_install_lat     = 246087800;            /* 0x22, 24.60878°N */
static int32_t  g_install_lng     = 1180689300;           /* 0x22, 118.06893°E */

/* 衍生定位字段（写入 INSTALL_LOCATION 或外部更新后同步） */
static uint8_t  g_gnss_fix        = 0x01;                 /* 0x23 */
static int32_t  g_latitude        = 246087800;            /* 0x24 */
static int32_t  g_longitude       = 1180689300;           /* 0x25 */
static int16_t  g_altitude        = 15;                   /* 0x26, m */

/* ═══════════════════════════════════════════════════════════════════════
 * 设备状态机（RAM-only，重启回 FACTORY）
 * ═══════════════════════════════════════════════════════════════════════ */

static uint8_t g_device_state      = DEVICE_STATE_FACTORY;
static bool    g_factory_init_done = false;

/* 全局时间戳（APP CMD 0x01 附带） */
static uint32_t g_unixtime;

/* ═══════════════════════════════════════════════════════════════════════
 * 字段权限判断
 * ═══════════════════════════════════════════════════════════════════════ */

/** @brief 字段是否可读 */
static bool field_is_readable(uint8_t field_id)
{
	switch (field_id) {
	case FIELD_DEVICE_STATE:
	case FIELD_DEVICE_TYPE:
	case FIELD_MODEL:
	case FIELD_SN:
	case FIELD_FW_VER:
	case FIELD_HW_VER:
	case FIELD_PROTOCOL_VERSION:
	case FIELD_BATTERY:
	case FIELD_TEMPERATURE:
	case FIELD_DISTANCE:
	case FIELD_POSITION:
	case FIELD_MODE:
	case FIELD_UPDATE_INTERVAL:
	case FIELD_INSTALL_LOCATION:
	case FIELD_GNSS_FIX:
	case FIELD_LATITUDE:
	case FIELD_LONGITUDE:
	case FIELD_ALTITUDE:
		return true;
	default:
		return false; /* 0x30~0x36 等预留字段 */
	}
}

/** @brief 字段是否可写 */
static bool field_is_writable(uint8_t field_id)
{
	switch (field_id) {
	case FIELD_MODE:
	case FIELD_UPDATE_INTERVAL:
	case FIELD_INSTALL_LOCATION:
		return true;
	default:
		return false;
	}
}

/* ═══════════════════════════════════════════════════════════════════════
 * 字段长度查询
 * ═══════════════════════════════════════════════════════════════════════ */

/** @brief 获取字段的固定值长度，0 表示变长 */
static uint8_t field_value_len(uint8_t field_id)
{
	switch (field_id) {
	case FIELD_DEVICE_STATE:      return 1;
	case FIELD_DEVICE_TYPE:       return 1;
	case FIELD_MODEL:             return 0; /* 变长 */
	case FIELD_SN:                return 0; /* 变长 */
	case FIELD_FW_VER:            return 3;
	case FIELD_HW_VER:            return 3;
	case FIELD_PROTOCOL_VERSION:  return 1;
	case FIELD_BATTERY:           return 1;
	case FIELD_TEMPERATURE:       return 2;
	case FIELD_DISTANCE:          return 2;
	case FIELD_POSITION:          return 1;
	case FIELD_MODE:              return 1;
	case FIELD_UPDATE_INTERVAL:   return 2;
	case FIELD_INSTALL_LOCATION:  return 8;
	case FIELD_GNSS_FIX:          return 1;
	case FIELD_LATITUDE:          return 4;
	case FIELD_LONGITUDE:         return 4;
	case FIELD_ALTITUDE:          return 2;
	default:                      return 0;
	}
}

/* ═══════════════════════════════════════════════════════════════════════
 * 大端序编解码辅助
 * ═══════════════════════════════════════════════════════════════════════ */

static void put_u16_be(uint8_t *buf, uint16_t v)
{
	buf[0] = (uint8_t)(v >> 8);
	buf[1] = (uint8_t)(v);
}

static void put_u32_be(uint8_t *buf, uint32_t v)
{
	buf[0] = (uint8_t)(v >> 24);
	buf[1] = (uint8_t)(v >> 16);
	buf[2] = (uint8_t)(v >> 8);
	buf[3] = (uint8_t)(v);
}

static uint16_t get_u16_be(const uint8_t *buf)
{
	return ((uint16_t)buf[0] << 8) | buf[1];
}

static uint32_t get_u32_be(const uint8_t *buf)
{
	return ((uint32_t)buf[0] << 24) |
	       ((uint32_t)buf[1] << 16) |
	       ((uint32_t)buf[2] << 8)  |
	       (uint32_t)buf[3];
}

/* ═══════════════════════════════════════════════════════════════════════
 * 传感器值外部更新接口（供 app_config.c setter 函数调用）
 * ═══════════════════════════════════════════════════════════════════════ */

void app_field_set_battery(uint8_t level)     { g_battery = level; }
void app_field_set_temperature(int16_t temp)   { g_temperature = temp; }
void app_field_set_distance(uint16_t dist)     { g_distance = dist; }
void app_field_set_position(uint8_t pos)       { g_position = pos; }
void app_field_set_gnss_fix(uint8_t fix)       { g_gnss_fix = fix; }
void app_field_set_latitude(int32_t lat)       { g_latitude = lat; }
void app_field_set_longitude(int32_t lng)      { g_longitude = lng; }
void app_field_set_altitude(int16_t alt)       { g_altitude = alt; }

/* ═══════════════════════════════════════════════════════════════════════
 * 公共 API
 * ═══════════════════════════════════════════════════════════════════════ */

void app_fields_init(void)
{
	/* 设备状态 */
	g_device_state      = DEVICE_STATE_FACTORY;
	g_factory_init_done = false;

	/* 可写配置字段恢复默认值 */
	g_mode            = 0x02;
	g_update_interval = 0;
	g_install_lat     = 246087800;
	g_install_lng     = 1180689300;

	/* 定位字段恢复默认 */
	g_gnss_fix  = 0x01;
	g_latitude  = 246087800;
	g_longitude = 1180689300;
	g_altitude  = 15;

	printk("[FIELDS] init OK, device_state=FACTORY, config defaults restored\n");
}

uint8_t app_field_get(uint8_t field_id, uint8_t *buf, uint8_t buf_max)
{
	if (!field_is_readable(field_id)) {
		return 0;
	}

	switch (field_id) {
	case FIELD_DEVICE_STATE:
		if (buf_max < 1) return 0;
		buf[0] = g_device_state;
		return 1;

	case FIELD_DEVICE_TYPE:
		if (buf_max < 1) return 0;
		buf[0] = g_device_type;
		return 1;

	case FIELD_MODEL: {
		uint8_t len = (uint8_t)strlen(g_model);
		if (len > buf_max) len = buf_max;
		memcpy(buf, g_model, len);
		return len;
	}
	case FIELD_SN: {
		uint8_t len = (uint8_t)strlen(g_sn);
		if (len > buf_max) len = buf_max;
		memcpy(buf, g_sn, len);
		return len;
	}
	case FIELD_FW_VER:
		if (buf_max < 3) return 0;
		memcpy(buf, g_fw_ver, 3);
		return 3;

	case FIELD_HW_VER:
		if (buf_max < 3) return 0;
		memcpy(buf, g_hw_ver, 3);
		return 3;

	case FIELD_PROTOCOL_VERSION:
		if (buf_max < 1) return 0;
		buf[0] = g_protocol_ver;
		return 1;

	case FIELD_BATTERY:
		if (buf_max < 1) return 0;
		buf[0] = g_battery;
		return 1;

	case FIELD_TEMPERATURE:
		if (buf_max < 2) return 0;
		put_u16_be(buf, (uint16_t)g_temperature);
		return 2;

	case FIELD_DISTANCE:
		if (buf_max < 2) return 0;
		put_u16_be(buf, g_distance);
		return 2;

	case FIELD_POSITION:
		if (buf_max < 1) return 0;
		buf[0] = g_position;
		return 1;

	case FIELD_MODE:
		if (buf_max < 1) return 0;
		buf[0] = g_mode;
		return 1;

	case FIELD_UPDATE_INTERVAL:
		if (buf_max < 2) return 0;
		put_u16_be(buf, g_update_interval);
		return 2;

	case FIELD_INSTALL_LOCATION:
		if (buf_max < 8) return 0;
		put_u32_be(buf,     (uint32_t)g_install_lat);
		put_u32_be(buf + 4, (uint32_t)g_install_lng);
		return 8;

	case FIELD_GNSS_FIX:
		if (buf_max < 1) return 0;
		buf[0] = g_gnss_fix;
		return 1;

	case FIELD_LATITUDE:
		if (buf_max < 4) return 0;
		put_u32_be(buf, (uint32_t)g_latitude);
		return 4;

	case FIELD_LONGITUDE:
		if (buf_max < 4) return 0;
		put_u32_be(buf, (uint32_t)g_longitude);
		return 4;

	case FIELD_ALTITUDE:
		if (buf_max < 2) return 0;
		put_u16_be(buf, (uint16_t)g_altitude);
		return 2;

	default:
		return 0;
	}
}

uint8_t app_field_validate(uint8_t field_id, const uint8_t *value, uint8_t len)
{
	/* 检查字段是否存在 */
	if (!field_is_readable(field_id)) {
		return APP_PROTO_STATUS_UNSUPPORTED;
	}

	/* 检查是否可写 */
	if (!field_is_writable(field_id)) {
		return APP_PROTO_STATUS_UNSUPPORTED;
	}

	/* 检查长度 */
	uint8_t expected_len = field_value_len(field_id);
	if (expected_len > 0 && len != expected_len) {
		return APP_PROTO_STATUS_PARAM_LEN;
	}

	/* 值域校验 */
	switch (field_id) {
	case FIELD_MODE:
		if (value[0] < 0x01 || value[0] > 0x03) {
			printk("[FIELDS] invalid mode 0x%02x\n", value[0]);
			return APP_PROTO_STATUS_PARAM_VALUE;
		}
		break;

	case FIELD_UPDATE_INTERVAL:
		/* 0~65535 均合法，仅检查长度 */
		break;

	case FIELD_INSTALL_LOCATION: {
		int32_t lat = (int32_t)get_u32_be(value);
		int32_t lng = (int32_t)get_u32_be(value + 4);

		if (lat < -900000000 || lat > 900000000 ||
		    lng < -1800000000 || lng > 1800000000) {
			printk("[FIELDS] invalid location: lat=%d, lng=%d\n", lat, lng);
			return APP_PROTO_STATUS_PARAM_VALUE;
		}
		break;
	}
	default:
		break;
	}

	return APP_PROTO_STATUS_OK;
}

void app_field_set(uint8_t field_id, const uint8_t *value, uint8_t len)
{
	switch (field_id) {
	case FIELD_MODE:
		g_mode = value[0];
		printk("[FIELDS] mode set to 0x%02x\n", g_mode);
		break;

	case FIELD_UPDATE_INTERVAL:
		g_update_interval = get_u16_be(value);
		printk("[FIELDS] interval set to %u min\n", g_update_interval);
		break;

	case FIELD_INSTALL_LOCATION: {
		g_install_lat = (int32_t)get_u32_be(value);
		g_install_lng = (int32_t)get_u32_be(value + 4);
		/* 写入安装位置后同步定位字段 */
		g_gnss_fix  = 0x01;
		g_latitude  = g_install_lat;
		g_longitude = g_install_lng;
		g_altitude  = 0;
		printk("[FIELDS] install_pos: lat=%d, lng=%d\n",
		       g_install_lat, g_install_lng);
		break;
	}
	default:
		break;
	}
}

uint8_t app_field_get_device_state(void)
{
	return g_device_state;
}

void app_field_set_device_state(uint8_t state)
{
	g_device_state = state;
	printk("[FIELDS] device_state -> 0x%02x\n", g_device_state);
}

bool app_field_is_factory_init_done(void)
{
	return g_factory_init_done;
}

void app_field_mark_factory_init_done(void)
{
	g_factory_init_done = true;
	if (g_device_state == DEVICE_STATE_FACTORY) {
		g_device_state = DEVICE_STATE_POWER_ON;
		printk("[FIELDS] factory init done, state -> POWER_ON\n");
	}
}

void app_field_set_unixtime(uint32_t ts)
{
	g_unixtime = ts;
}

uint32_t app_field_get_unixtime(void)
{
	return g_unixtime;
}

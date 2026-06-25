/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/types.h>
#include <zephyr/kernel.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>

#include <bluetooth/services/nus.h>

#if defined(CONFIG_DK_LIBRARY)
#include <dk_buttons_and_leds.h>
#endif

#include <zephyr/settings/settings.h>

#include <zephyr/logging/log.h>

#define LOG_MODULE_NAME ble_uart
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#include "ble_uart.h"
#include "app_config.h"
#include "app_auth.h"

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#if defined(CONFIG_DK_LIBRARY)
#define BLE_CON_LED         DK_LED2
#define KEY_PASSKEY_ACCEPT  DK_BTN1_MSK
#define KEY_PASSKEY_REJECT  DK_BTN2_MSK
#endif

static struct bt_conn *current_conn;
static struct bt_conn *auth_conn;
static struct k_work adv_work;

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_VAL),
};

/* BLE响应发送：等CCCD订阅后再发送 */
static bool nus_tx_subscribed;
static bool resp_pending;
static uint16_t resp_len_pending;
static struct bt_conn *resp_conn;
static uint8_t resp_buf[APP_CONFIG_RESPONSE_PAYLOAD_SIZE];

static void send_retry_handler(struct k_work *work);

static K_WORK_DELAYABLE_DEFINE(send_retry_work, send_retry_handler);

#define RESP_RETRY_MAX 30

static void send_pending_resp(void)
{
	static uint8_t retry_cnt;
	if (!resp_conn || !resp_pending) {
		return;
	}

	int err = bt_nus_send(resp_conn, resp_buf, resp_len_pending);
	if (err) {
		retry_cnt++;
		if (retry_cnt >= RESP_RETRY_MAX) {
			LOG_ERR("BLE response failed after %d retries: ATT MTU too small (need ≥66). "
				"APP must request MTU > 66 after connection.", retry_cnt);
			resp_pending = false;
			retry_cnt = 0;
		} else {
			k_work_schedule(&send_retry_work, K_MSEC(100));
		}
	} else {
		LOG_INF("Response sent successfully");
		resp_pending = false;
		retry_cnt = 0;
	}
}

static void send_retry_handler(struct k_work *work)
{
	send_pending_resp();
}

static void nus_send_enabled_cb(enum bt_nus_send_status status)
{
	nus_tx_subscribed = (status == BT_NUS_SEND_STATUS_ENABLED);
	LOG_INF("NUS TX notifications %s",
		nus_tx_subscribed ? "enabled" : "disabled");

	if (nus_tx_subscribed) {
		send_pending_resp();
	}
}

static void adv_work_handler(struct k_work *work)
{
	int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));

	if (err) {
		LOG_ERR("Advertising failed to start (err %d)", err);
		return;
	}

	LOG_INF("Advertising successfully started");
}

static void advertising_start(void)
{
	k_work_submit(&adv_work);
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	if (err) {
		LOG_ERR("Connection failed, err 0x%02x %s", err, bt_hci_err_to_str(err));
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("Connected %s", addr);

	current_conn = bt_conn_ref(conn);

#if defined(CONFIG_DK_LIBRARY)
	dk_set_led_on(BLE_CON_LED);
#endif
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Disconnected: %s, reason 0x%02x %s", addr, reason, bt_hci_err_to_str(reason));

	if (auth_conn) {
		bt_conn_unref(auth_conn);
		auth_conn = NULL;
	}

	if (current_conn) {
		bt_conn_unref(current_conn);
		current_conn = NULL;
#if defined(CONFIG_DK_LIBRARY)
		dk_set_led_off(BLE_CON_LED);
#endif
	}

	resp_conn = NULL;
	resp_pending = false;
	nus_tx_subscribed = false;

	/* 清除 BLE 认证解锁状态 */
	app_auth_ble_disconnect();
}

static void recycled_cb(void)
{
	LOG_INF("Connection object available from previous conn. Disconnect is complete!");
	advertising_start();
}

#ifdef CONFIG_BT_NUS_SECURITY_ENABLED
static void security_changed(struct bt_conn *conn, bt_security_t level,
			     enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (!err) {
		LOG_INF("Security changed: %s level %u", addr, level);
	} else {
		LOG_WRN("Security failed: %s level %u err %s", addr, level,
			bt_security_err_to_str(err));
	}
}
#endif

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected        = connected,
	.disconnected     = disconnected,
	.recycled         = recycled_cb,
#ifdef CONFIG_BT_NUS_SECURITY_ENABLED
	.security_changed = security_changed,
#endif
};

#if defined(CONFIG_BT_NUS_SECURITY_ENABLED)
static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Passkey for %s: %06u", addr, passkey);
}

static void auth_passkey_confirm(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];

	auth_conn = bt_conn_ref(conn);

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Passkey for %s: %06u", addr, passkey);

	if (IS_ENABLED(CONFIG_SOC_SERIES_NRF54H) || IS_ENABLED(CONFIG_SOC_SERIES_NRF54L)) {
		LOG_INF("Press Button 0 to confirm, Button 1 to reject.");
	} else {
		LOG_INF("Press Button 1 to confirm, Button 2 to reject.");
	}
}

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Pairing cancelled: %s", addr);
}

static void pairing_complete(struct bt_conn *conn, bool bonded)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Pairing completed: %s, bonded: %d", addr, bonded);
}

static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Pairing failed conn: %s, reason %d %s", addr, reason,
		bt_security_err_to_str(reason));
}

static struct bt_conn_auth_cb conn_auth_callbacks = {
	.passkey_display = auth_passkey_display,
	.passkey_confirm = auth_passkey_confirm,
	.cancel = auth_cancel,
};

static struct bt_conn_auth_info_cb conn_auth_info_callbacks = {
	.pairing_complete = pairing_complete,
	.pairing_failed = pairing_failed
};

#if defined(CONFIG_DK_LIBRARY)
static void num_comp_reply(bool accept)
{
	if (accept) {
		bt_conn_auth_passkey_confirm(auth_conn);
		LOG_INF("Numeric Match, conn %p", (void *)auth_conn);
	} else {
		bt_conn_auth_cancel(auth_conn);
		LOG_INF("Numeric Reject, conn %p", (void *)auth_conn);
	}

	bt_conn_unref(auth_conn);
	auth_conn = NULL;
}

static void button_changed(uint32_t button_state, uint32_t has_changed)
{
	uint32_t buttons = button_state & has_changed;

	if (auth_conn) {
		if (buttons & KEY_PASSKEY_ACCEPT) {
			num_comp_reply(true);
		}

		if (buttons & KEY_PASSKEY_REJECT) {
			num_comp_reply(false);
		}
	}
}
#endif /* CONFIG_DK_LIBRARY */
#else
static struct bt_conn_auth_cb conn_auth_callbacks;
static struct bt_conn_auth_info_cb conn_auth_info_callbacks;
#endif /* CONFIG_BT_NUS_SECURITY_ENABLED */

static void bt_receive_cb(struct bt_conn *conn, const uint8_t *const data,
			  uint16_t len)
{
	char addr[BT_ADDR_LE_STR_LEN] = {0};

	if (len < 1) {
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, ARRAY_SIZE(addr));
	LOG_INF("Received from %s, len=%u, cmd=0x%02x", addr, len, data[0]);

	uint16_t resp_len = app_config_handle_ble(data, len, resp_buf);
	if (resp_len > 0) {
		resp_conn = conn;
		resp_pending = true;
		resp_len_pending = resp_len;
		/* 延迟发送，确保 ATT 通知通道完全就绪 */
		k_work_schedule(&send_retry_work, K_MSEC(500));
	}

	if (resp_len > 0 && !nus_tx_subscribed) {
		LOG_INF("NUS TX not subscribed yet, will retry after subscribe");
	}
}

static struct bt_nus_cb nus_cb = {
	.received = bt_receive_cb,
	.send_enabled = nus_send_enabled_cb,
};

int ble_uart_init(void)
{
	int err;

#if defined(CONFIG_BT_NUS_SECURITY_ENABLED) && defined(CONFIG_DK_LIBRARY)
	err = dk_buttons_init(button_changed);
	if (err) {
		LOG_ERR("Cannot init buttons (err: %d)", err);
		return err;
	}
#endif

	if (IS_ENABLED(CONFIG_BT_NUS_SECURITY_ENABLED)) {
		err = bt_conn_auth_cb_register(&conn_auth_callbacks);
		if (err) {
			LOG_ERR("Failed to register authorization callbacks. (err: %d)", err);
			return err;
		}

		err = bt_conn_auth_info_cb_register(&conn_auth_info_callbacks);
		if (err) {
			LOG_ERR("Failed to register authorization info callbacks. (err: %d)", err);
			return err;
		}
	}

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("bt_enable failed (err: %d)", err);
		return err;
	}

	LOG_INF("Bluetooth initialized");

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	err = bt_nus_init(&nus_cb);
	if (err) {
		LOG_ERR("Failed to initialize UART service (err: %d)", err);
		return err;
	}

	k_work_init(&adv_work, adv_work_handler);
	advertising_start();

	return 0;
}

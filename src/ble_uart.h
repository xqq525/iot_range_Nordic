/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef BLE_UART_H__
#define BLE_UART_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize BLE UART service (NUS).
 *
 *        Initializes UART, Bluetooth stack, NUS service, and starts
 *        advertising. A dedicated thread forwards UART RX data to BLE.
 *
 * @return 0 on success, negative on error.
 */
int ble_uart_init(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_UART_H__ */

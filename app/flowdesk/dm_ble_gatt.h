/*
 * dm_ble_gatt.h — FlowDesk BLE GATT server (App pairing)
 * Protocol matches Android BleProtocol.kt / dm_bt_protocol.h
 */

#ifndef DM_BLE_GATT_H
#define DM_BLE_GATT_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  DM_BLE_IDLE = 0,
  DM_BLE_STARTING,
  DM_BLE_ADVERTISING,
  DM_BLE_CONNECTED,
  DM_BLE_PAIRING,
  DM_BLE_PAIRED,
  DM_BLE_ERROR,
} dm_ble_state_t;

int dm_ble_start(void);
void dm_ble_stop(void);
dm_ble_state_t dm_ble_state(void);
bool dm_ble_is_paired(void);
int dm_ble_paired_addr(char *buf, int buflen);

/* callbacks installed by UI (optional) */
typedef void (*dm_ble_state_cb_t)(dm_ble_state_t st);
void dm_ble_set_state_cb(dm_ble_state_cb_t cb);

#endif /* DM_BLE_GATT_H */

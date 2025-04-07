#ifndef ESTC_SERVICE_H__
#define ESTC_SERVICE_H__

#include <stdint.h>

#include "sdk_errors.h"
#include "blinky_types.h"

// Service 128-bit UUID (Version 4 UUID)
#define ESTC_SERVICE_UUID_128 { 0x91, 0x30, 0x4b, 0x4c, 0xf2, 0x2a, /* - */ 0x42, 0x43, /* - */ 0x95, 0xd8, /* - */ 0xf6, 0xc8, /* - */ 0x47, 0x1e, 0x92, 0xb3 }

// Service 16-bit UUID
#define ESTC_SERVICE_UUID_16 0xdead

// Characteristic 16-bit UUID
#define ESTC_CHAR_1_UUID_16 0xdea1
#define ESTC_CHAR_2_UUID_16 0xdea2

#define ESTC_CHAR_LED_RGB_STATE_LEN   4

ret_code_t estc_ble_service_init(ble_estc_service_t *service);

void estc_ble_service_on_ble_event(const ble_evt_t *ble_evt, void *ctx);

void estc_ble_set_value(ble_estc_service_t* service, uint8_t* data, uint16_t len);

#endif /* ESTC_SERVICE_H__ */
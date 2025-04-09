/**
 * Copyright 2022 Evgeniy Morozov
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE
*/

#ifndef ESTC_SERVICE_H__
#define ESTC_SERVICE_H__

#include <stdint.h>

#include "ble.h"
#include "sdk_errors.h"

#define ESTC_CHAR_LEN   20  

// Service 128-bit UUID (Version 4 UUID)
#define ESTC_SERVICE_UUID_128 { 0x91, 0x30, 0x4b, 0x4c, 0xf2, 0x2a, /* - */ 0x42, 0x43, /* - */ 0x95, 0xd8, /* - */ 0xf6, 0xc8, /* - */ 0x47, 0x1e, 0x92, 0xb3 }

// Service 16-bit UUID
#define ESTC_SERVICE_UUID_16 0xdead

// Characteristic 16-bit UUID
#define ESTC_CHAR_1_UUID_16 0x0001
#define ESTC_CHAR_2_UUID_16 0x0002
#define ESTC_CHAR_3_UUID_16 0x0003

typedef struct
{
    uint16_t service_handle;
    uint16_t connection_handle;
    ble_gatts_char_handles_t characterstic1_handle;
    ble_gatts_char_handles_t characterstic2_handle;
    ble_gatts_char_handles_t characterstic3_handle;
} ble_estc_service_t;

ret_code_t estc_ble_service_init(ble_estc_service_t *service);

void estc_ble_service_on_ble_event(const ble_evt_t *ble_evt, void *ctx);

void estc_update_characteristic1_value(ble_estc_service_t *service, uint8_t* data, uint16_t len);
void estc_update_characteristic2_value(ble_estc_service_t *service, uint8_t *data, uint16_t len);
void estc_update_characteristic3_value(ble_estc_service_t *service, uint8_t *data, uint16_t len);

#endif /* ESTC_SERVICE_H__ */
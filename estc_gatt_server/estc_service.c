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

#include "estc_service.h"

#include "app_error.h"
#include "nrf_log.h"

#include "ble.h"
#include "ble_gatts.h"
#include "ble_srv_common.h"

static uint8_t          m_char1_value[ESTC_CHAR_LEN] = { 0 };
static uint8_t          m_char2_value[ESTC_CHAR_LEN] = { 0 };
static uint8_t          m_char3_value[ESTC_CHAR_LEN] = { 0 };

static uint8_t                  m_char_desc[] = "Mercedes GLK";

static ret_code_t estc_ble_add_characteristics(ble_estc_service_t *service, const uint8_t uuid_type);

ret_code_t estc_ble_service_init(ble_estc_service_t *service)
{
    ASSERT(NULL != service)
    ret_code_t error_code = NRF_SUCCESS;
    ble_uuid128_t base_uuid = {ESTC_SERVICE_UUID_128};
    ble_uuid_t service_uuid;

    service_uuid.uuid = ESTC_SERVICE_UUID_16; /**< 16-bit UUID value or octets 12-13 of 128-bit UUID. */

    // Add service UUIDs to the BLE stack table
    error_code = sd_ble_uuid_vs_add(&base_uuid, &service_uuid.type);
    APP_ERROR_CHECK(error_code);

    // Add service to the BLE stack
    error_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &service_uuid, &service->service_handle);
    APP_ERROR_CHECK(error_code);

    // NRF_LOG_DEBUG("%s:%d | Service UUID: 0x%04x", __FUNCTImakeON__, __LINE__, service_uuid.uuid);
    // NRF_LOG_DEBUG("%s:%d | Service UUID type: 0x%02x", __FUNCTION__, __LINE__, service_uuid.type);
    // NRF_LOG_DEBUG("%s:%d | Service handle: 0x%04x", __FUNCTION__, __LINE__, service->service_handle);

    return estc_ble_add_characteristics(service, service_uuid.type);
}

static ret_code_t estc_ble_add_characteristics(ble_estc_service_t *service, const uint8_t uuid_type)
{
    ASSERT(NULL != service)
    ret_code_t error_code = NRF_SUCCESS;
    ble_uuid_t characteristic1_uuid;
    ble_uuid_t characteristic2_uuid;
    ble_uuid_t characteristic3_uuid;
      
    characteristic1_uuid.uuid = ESTC_CHAR_1_UUID_16; /**< 16-bit UUID value or octets 12-13 of 128-bit UUID. */
    characteristic1_uuid.type = uuid_type;
    characteristic2_uuid.uuid = ESTC_CHAR_2_UUID_16;
    characteristic2_uuid.type = uuid_type;
    characteristic3_uuid.uuid = ESTC_CHAR_3_UUID_16;
    characteristic3_uuid.type = uuid_type;

    // Configure Characteristic metadata (enable read and write)
    ble_gatts_char_md_t char1_md = { 0 };
    char1_md.char_props.read  = 1;
    char1_md.char_props.write = 1;

    ble_gatts_char_md_t char2_md = { 0 };
    char2_md.char_props.read  = 1;
    char2_md.char_props.indicate = 1;

    ble_gatts_char_md_t char3_md = { 0 };    
    char3_md.char_props.read  = 1;
    char3_md.char_props.notify = 1;

    // Additional descriptor
    char1_md.p_char_user_desc = m_char_desc;
    char1_md.char_user_desc_max_size = sizeof(m_char_desc);
    char1_md.char_user_desc_size = sizeof(m_char_desc);

    // Configures attribute metadata. For now we only specify that the attribute will be stored in the softdevice
    ble_gatts_attr_md_t attr1_md = { 0 };
    attr1_md.vloc = BLE_GATTS_VLOC_STACK;
    
    ble_gatts_attr_md_t attr2_md = { 0 };
    attr2_md.vloc = BLE_GATTS_VLOC_STACK;

    ble_gatts_attr_md_t attr3_md = { 0 };
    attr3_md.vloc = BLE_GATTS_VLOC_STACK;

    // Set read/write security levels to our attribute metadata
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr1_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr1_md.write_perm);

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr2_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr2_md.write_perm);

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr3_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr3_md.write_perm);

    // Configure the characteristic value attribute
    ble_gatts_attr_t attr_char1_value = { 0 };
    attr_char1_value.p_uuid = &characteristic1_uuid;
    attr_char1_value.p_attr_md = &attr1_md;

    ble_gatts_attr_t attr_char2_value = { 0 };
    attr_char2_value.p_uuid = &characteristic2_uuid;
    attr_char2_value.p_attr_md = &attr2_md;

    ble_gatts_attr_t attr_char3_value = { 0 };
    attr_char3_value.p_uuid = &characteristic3_uuid;
    attr_char3_value.p_attr_md = &attr3_md;

    m_char1_value[0] = 'A';
    m_char1_value[1] = 'n';
    m_char1_value[2] = 'd';
    m_char1_value[3] = 'r';
    m_char1_value[4] = 'e';
    m_char1_value[5] = 'w';

    m_char2_value[0] = 'X';

    m_char3_value[0] = 'Y';

    // Set characteristic length in number of bytes in attr_char_value structure
    attr_char1_value.init_len  = ESTC_CHAR_LEN;
    attr_char1_value.init_offs = 0;
    attr_char1_value.max_len   = ESTC_CHAR_LEN;
    attr_char1_value.p_value   = m_char1_value;

    attr_char2_value.init_len  = ESTC_CHAR_LEN;
    attr_char2_value.init_offs = 0;
    attr_char2_value.max_len   = ESTC_CHAR_LEN;
    attr_char2_value.p_value   = m_char2_value;

    attr_char3_value.init_len  = ESTC_CHAR_LEN;
    attr_char3_value.init_offs = 0;
    attr_char3_value.max_len   = ESTC_CHAR_LEN;
    attr_char3_value.p_value   = m_char3_value;

    // Add new characteristic to the service
    error_code = sd_ble_gatts_characteristic_add(service->service_handle,
                                               &char1_md,
                                               &attr_char1_value,
                                               &(service->characterstic1_handle));

    error_code = sd_ble_gatts_characteristic_add(service->service_handle,
                                               &char2_md,
                                               &attr_char2_value,
                                               &(service->characterstic2_handle));

    error_code = sd_ble_gatts_characteristic_add(service->service_handle,
                                               &char3_md,
                                               &attr_char3_value,
                                               &(service->characterstic3_handle));

    APP_ERROR_CHECK(error_code);
    return NRF_SUCCESS;
}

void estc_update_characteristic1_value(ble_estc_service_t *service, uint8_t* data, uint16_t len)
{
    NRF_LOG_INFO("estc_update_characteristic1_value");
    ASSERT(NULL != service)
    ASSERT(NULL != data)

    if ((service->connection_handle != BLE_CONN_HANDLE_INVALID) && (len <= ESTC_CHAR_LEN))
    {
        ble_gatts_value_t value;
        value.len = len;
        value.offset = 0;
        value.p_value = data;
        NRF_LOG_INFO("sd_ble_gatts_value_set");
        sd_ble_gatts_value_set(service->connection_handle, service->characterstic1_handle.value_handle, &value);
    }
}

void estc_update_characteristic2_value(ble_estc_service_t *service, uint8_t *data, uint16_t len)
{
    NRF_LOG_INFO("estc_update_characteristic2_value");
    ASSERT(NULL != service)
    ASSERT(NULL != data)
    ble_gatts_hvx_params_t hvx_params = { 0 };
    hvx_params.handle = service->characterstic2_handle.value_handle;
    hvx_params.type = BLE_GATT_HVX_INDICATION;

    hvx_params.p_len = &len;
    hvx_params.p_data = data;

    if ((service->connection_handle != BLE_CONN_HANDLE_INVALID) && (len <= ESTC_CHAR_LEN))
    {
        NRF_LOG_INFO("sd_ble_gatts_hvx BLE_GATT_HVX_INDICATION");
        sd_ble_gatts_hvx(service->connection_handle, &hvx_params);
    }
}

void estc_update_characteristic3_value(ble_estc_service_t *service, uint8_t *data, uint16_t len)
{
    NRF_LOG_INFO("estc_update_characteristic3_value");
    ASSERT(NULL != service)
    ASSERT(NULL != data)
    ble_gatts_hvx_params_t hvx_params = { 0 };
    hvx_params.handle = service->characterstic3_handle.value_handle;
    hvx_params.type = BLE_GATT_HVX_NOTIFICATION;

    hvx_params.p_len = &len;
    hvx_params.p_data = data;

    if ((service->connection_handle != BLE_CONN_HANDLE_INVALID) && (len <= ESTC_CHAR_LEN))
    {
        NRF_LOG_INFO("sd_ble_gatts_hvx BLE_GATT_HVX_NOTIFICATION");
        sd_ble_gatts_hvx(service->connection_handle, &hvx_params);
    }
}
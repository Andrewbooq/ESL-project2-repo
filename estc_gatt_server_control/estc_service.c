#include "estc_service.h"

#include "app_error.h"
#include "nrf_log.h"

#include "ble.h"
#include "ble_gatts.h"
#include "ble_srv_common.h"

static uint8_t          m_char_value[ESTC_CHAR_LED_RGB_STATE_LEN] = { 0 };
static uint8_t          m_char1_desc[] = "LED RGB color & state: 3 bytes of RGB & 1 byte of STATE";

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

    return estc_ble_add_characteristics(service, service_uuid.type);
}

static ret_code_t estc_ble_add_characteristics(ble_estc_service_t *service, const uint8_t uuid_type)
{
    ASSERT(NULL != service)
    ret_code_t error_code = NRF_SUCCESS;
    ble_uuid_t characteristic1_uuid;
    characteristic1_uuid.uuid = ESTC_CHAR_1_UUID_16;
    characteristic1_uuid.type = uuid_type;

    ble_uuid_t characteristic2_uuid;
    characteristic2_uuid.uuid = ESTC_CHAR_2_UUID_16;
    characteristic2_uuid.type = uuid_type;


    ble_gatts_attr_md_t cccd_md = { 0 };
    cccd_md.vloc = BLE_GATTS_VLOC_STACK;
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);

    // Configure Characteristic metadata (enable read and write)
    ble_gatts_char_md_t char1_md     = { 0 };
    char1_md.char_props.read         = 1;
    char1_md.char_props.write        = 1;
    char1_md.p_char_user_desc        = m_char1_desc; // Additional descriptor
    char1_md.char_user_desc_max_size = sizeof(m_char1_desc);
    char1_md.char_user_desc_size     = sizeof(m_char1_desc);
    char1_md.p_cccd_md               = &cccd_md;

    ble_gatts_char_md_t char2_md = { 0 };
    char2_md.char_props.read  = 1;
    char2_md.char_props.notify = 1;

    // Configures attribute metadata. For now we only specify that the attribute will be stored in the softdevice
    ble_gatts_attr_md_t attr1_md = { 0 };
    attr1_md.vloc = BLE_GATTS_VLOC_STACK;
    
    ble_gatts_attr_md_t attr2_md = { 0 };
    attr2_md.vloc = BLE_GATTS_VLOC_STACK;

    // Set read/write security levels to our attribute metadata
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr1_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr1_md.write_perm);

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr2_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr2_md.write_perm);


    // Configure the characteristic value attribute
    ble_gatts_attr_t attr_char1_value = { 0 };
    attr_char1_value.p_uuid    = &characteristic1_uuid;
    attr_char1_value.p_attr_md = &attr1_md;
    attr_char1_value.init_len  = ESTC_CHAR_LED_RGB_STATE_LEN;
    attr_char1_value.init_offs = 0;
    attr_char1_value.max_len   = ESTC_CHAR_LED_RGB_STATE_LEN;
    attr_char1_value.p_value   = m_char_value;

    error_code = sd_ble_gatts_characteristic_add(service->service_handle,
                                               &char1_md,
                                               &attr_char1_value,
                                               &(service->rgb_state_handle));
    APP_ERROR_CHECK(error_code);

    ble_gatts_attr_t attr_char2_value = { 0 };
    attr_char2_value.p_uuid    = &characteristic2_uuid;
    attr_char2_value.p_attr_md = &attr2_md;
    attr_char2_value.init_len  = ESTC_CHAR_LED_RGB_STATE_LEN;
    attr_char2_value.init_offs = 0;
    attr_char2_value.max_len   = ESTC_CHAR_LED_RGB_STATE_LEN;
    attr_char2_value.p_value   = m_char_value;
    error_code = sd_ble_gatts_characteristic_add(service->service_handle,
                                               &char2_md,
                                               &attr_char2_value,
                                               &(service->notifiction_handle));
    APP_ERROR_CHECK(error_code);

    return NRF_SUCCESS;
}

void estc_ble_send_notification(ble_estc_service_t* service, uint8_t* data, uint16_t len)
{
    NRF_LOG_INFO("BLE: estc_ble_send_notification");
    ASSERT(NULL != service)
    ASSERT(NULL != data)
    ble_gatts_hvx_params_t hvx_params = { 0 };
    hvx_params.handle = service->notifiction_handle.value_handle;
    hvx_params.type = BLE_GATT_HVX_NOTIFICATION;

    hvx_params.p_len = &len;
    hvx_params.p_data = data;

    if (service->connection_handle != BLE_CONN_HANDLE_INVALID)
    {
        sd_ble_gatts_hvx(service->connection_handle, &hvx_params);
    }
}

void estc_ble_update_attribute(ble_estc_service_t* service, uint8_t* data, uint16_t len)
{
    NRF_LOG_INFO("BLE: estc_ble_update_attribute");
    ASSERT(NULL != service)
    ASSERT(NULL != data)

    if (service->connection_handle != BLE_CONN_HANDLE_INVALID)
    {
        ble_gatts_value_t value;
        value.len = len;
        value.offset = 0;
        value.p_value = data;

        sd_ble_gatts_value_set(service->connection_handle, service->rgb_state_handle.value_handle, &value);
    }
}

void estc_ble_set_value(ble_estc_service_t* service, uint8_t* data, uint16_t len)
{
    NRF_LOG_INFO("BLE: estc_ble_set_value");
    if (ESTC_CHAR_LED_RGB_STATE_LEN != len)
    {
        NRF_LOG_INFO("BLE: estc_ble_set_value: invalid value len");
        return;
    }
    memcpy(m_char_value, data, len);

    estc_ble_send_notification(service, data, len);
    estc_ble_update_attribute(service, data, len);
}
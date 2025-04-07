#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "nordic_common.h"
#include "nrf.h"
#include "app_error.h"
#include "ble.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh_ble.h"
#include "app_timer.h"
#include "fds.h"
#include "peer_manager.h"
#include "peer_manager_handler.h"
#include "bsp_btn_ble.h"
#include "sensorsim.h"
#include "ble_conn_state.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "nrf_pwr_mgmt.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrf_log_backend_usb.h"

#include "blinky_types.h"
#include "blinky_led.h"
#include "blinky_led_pwm.h"
#include "blinky_led_soft.h"
#include "blinky_color.h"
#include "blinky_nvmc.h"
#include "blinky_btn.h"
#include "estc_service.h"

#define DEVICE_NAME                     "ESTC-GATT"                             /**< Name of device. Will be included in the advertising data. */
#define MANUFACTURER_NAME               "NordicSemiconductor"                   /**< Manufacturer. Will be passed to Device Information Service. */
#define APP_ADV_INTERVAL                300                                     /**< The advertising interval (in units of 0.625 ms. This value corresponds to 187.5 ms). */

#define APP_ADV_DURATION                18000                                   /**< The advertising duration (180 seconds) in units of 10 milliseconds. */
#define APP_BLE_OBSERVER_PRIO           3                                       /**< Application's BLE observer priority. You shouldn't need to modify this value. */
#define APP_BLE_CONN_CFG_TAG            1                                       /**< A tag identifying the SoftDevice BLE configuration. */

#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(100, UNIT_1_25_MS)        /**< Minimum acceptable connection interval (0.1 seconds). */
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(200, UNIT_1_25_MS)        /**< Maximum acceptable connection interval (0.2 second). */
#define SLAVE_LATENCY                   0                                       /**< Slave latency. */
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(4000, UNIT_10_MS)         /**< Connection supervisory timeout (4 seconds). */

#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000)                   /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000)                  /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT    3                                       /**< Number of attempts before giving up the connection parameter negotiation. */

#define DEAD_BEEF                       0xDEADBEEF                              /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

/* stock number of a board (DEVICE_ID=#ABCD) */
#define BLINKY_SN_A         6
#define BLINKY_SN_B         5
#define BLINKY_SN_C         8
#define BLINKY_SN_D         4

#define BLINKY_STATE_FAST_BLINK_MS  800
#define BLINKY_STATE_SLOW_BLINK_MS  3000

#define BLINKY_VELOCITY_MS          50 /* timeout between steps of moving through colors */

APP_TIMER_DEF(g_timer_move);

NRF_BLE_GATT_DEF(m_gatt);                                                       /**< GATT module instance. */
NRF_BLE_QWR_DEF(m_qwr);                                                         /**< Context for the Queued Write module.*/
BLE_ADVERTISING_DEF(m_advertising);                                             /**< Advertising module instance. */

static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;                        /**< Handle of the current connection. */

static ble_uuid_t m_adv_uuids[] =                                               /**< Universally unique service identifiers. */
{
    {BLE_UUID_DEVICE_INFORMATION_SERVICE, BLE_UUID_TYPE_BLE},
    {ESTC_SERVICE_UUID_16, BLE_UUID_TYPE_BLE},
};

static data_t g_data =
{
    .state = T_VIEW,
    .move_s_up = false,
    .move_v_up = false,
    .need_save = false,
    .current = 
    {
        .hsv = 
        {
            /* DEVICE_ID=6584
            Last digits: 84
            Hue: 84% => 360 * 0.84 = 302° */
            .h = ((BLINKY_SN_C * 10.f) + BLINKY_SN_D)  * 360.f / 100.f,
            .s = 100.f,
            .v = 100.f
        },
        .state = true,
    },
    .saved = 
    {
        .hsv = 
        {
            .h = 50.f,
            .s = 50.f,
            .v = 50.f 
        },
        .state = true,
    },
    .estc_ble_service = { 0 },
};

static void advertising_start(void);
static void blinky_set_led_rgb(rgb_t* rgb, bool state);

/**@brief Callback function for asserts in the SoftDevice.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in] line_num   Line number of the failing ASSERT call.
 * @param[in] file_name  File name of the failing ASSERT call.
 */
//void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
//{
//    app_error_handler(DEAD_BEEF, line_num, p_file_name);
//}

/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module. This creates and starts application timers.
 */
static void timers_init(void)
{
    // Initialize timer module.
    ret_code_t err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for the GAP initialization.
 *
 * @details This function sets up all the necessary GAP (Generic Access Profile) parameters of the
 *          device including the device name, appearance, and the preferred connection parameters.
 */
static void gap_params_init(void)
{
    ret_code_t              err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *)DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

	err_code = sd_ble_gap_appearance_set(BLE_APPEARANCE_UNKNOWN);
	APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the GATT module.
 */
static void gatt_init(void)
{
    ret_code_t err_code = nrf_ble_gatt_init(&m_gatt, NULL);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling Queued Write Module errors.
 *
 * @details A pointer to this function will be passed to each service which may need to inform the
 *          application about an error.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void nrf_qwr_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}

/**@brief Function for initializing services that will be used by the application.
 */
static void services_init(void)
{
    ret_code_t         err_code;
    nrf_ble_qwr_init_t qwr_init = {0};

    // Initialize Queued Write Module.
    qwr_init.error_handler = nrf_qwr_error_handler;

    err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
    APP_ERROR_CHECK(err_code);

    err_code = estc_ble_service_init(&g_data.estc_ble_service);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module which
 *          are passed to the application.
 *          @note All this function does is to disconnect. This could have been done by simply
 *                setting the disconnect_on_fail config parameter, but instead we use the event
 *                handler mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    ret_code_t err_code;

    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err_code);
    }
}


/**@brief Function for handling a Connection Parameters error.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void)
{
    ret_code_t             err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for starting timers.
 */
static void application_timers_start(void)
{
}


/**@brief Function for putting the chip into sleep mode.
 *
 * @note This function will not return.
 */
static void sleep_mode_enter(void)
{
    ret_code_t err_code;

    err_code = bsp_indication_set(BSP_INDICATE_IDLE);
    APP_ERROR_CHECK(err_code);

    // Prepare wakeup buttons.
    err_code = bsp_btn_ble_sleep_mode_prepare();
    APP_ERROR_CHECK(err_code);

    // Go to system-off mode (this function will not return; wakeup will cause a reset).
    err_code = sd_power_system_off();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    ret_code_t err_code;

    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_FAST:
            NRF_LOG_INFO("ADV Event: Start fast advertising");
            err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_ADV_EVT_IDLE:
            NRF_LOG_INFO("ADV Event: idle, no connectable advertising is ongoing");
            sleep_mode_enter();
            break;

        default:
            break;
    }
}

static void blinky_ble_on_write(const ble_evt_t* ble_evt, data_t* data)
{
    NRF_LOG_INFO("MAIN: blinky_ble_on_write");
    ble_estc_service_t *service = &(data->estc_ble_service);
    const ble_gatts_evt_write_t * p_evt_write = &ble_evt->evt.gatts_evt.params.write;
    // check our handle
    if (p_evt_write->handle == service->rgb_state_handle.value_handle)
    {
        NRF_LOG_INFO("MAIN: blinky_ble_on_write: len:%u", p_evt_write->len);
        // check the lenth
        if(ESTC_CHAR_LED_RGB_STATE_LEN == p_evt_write->len)
        {
            NRF_LOG_INFO("MAIN: blinky_ble_on_write: R:%u, G:%u, B:%u, STATE:%u", p_evt_write->data[0],
                                                                                  p_evt_write->data[1],
                                                                                  p_evt_write->data[2],
                                                                                  p_evt_write->data[3]);

            // apply color and state
            rgb_t rgb = { 0.f };
            // transform [0..255] to [0..100]
            rgb.r = (float)(p_evt_write->data[0] * 100.f / 255.f);
            rgb.g = (float)(p_evt_write->data[1] * 100.f / 255.f);
            rgb.b = (float)(p_evt_write->data[2] * 100.f / 255.f);
            rgb2hsv(&rgb, (hsv_t*)&(g_data.current.hsv));
            g_data.current.state = (bool)(p_evt_write->data[3]);
            blinky_set_led_rgb(&rgb, g_data.current.state);

            uint8_t rgb_state_value[ESTC_CHAR_LED_RGB_STATE_LEN] = { 0 };
            rgb_state_value[0] = p_evt_write->data[0];
            rgb_state_value[1] = p_evt_write->data[1];
            rgb_state_value[2] = p_evt_write->data[2];
            rgb_state_value[3] = p_evt_write->data[3];
            estc_ble_set_value(service, rgb_state_value, ESTC_CHAR_LED_RGB_STATE_LEN);
            g_data.need_save = true;
        }
        else
        {
            NRF_LOG_INFO("MAIN: blinky_ble_on_write: Incorrect lenth");
        }
    }
}

/**@brief Function for handling BLE events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 * @param[in]   p_context   Unused.
 */
static void ble_evt_handler(ble_evt_t const* p_ble_evt, void* p_context)
{
    ret_code_t err_code = NRF_SUCCESS;
    data_t* data = (data_t*) p_context;
    ble_estc_service_t* service = &(data->estc_ble_service);

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_DISCONNECTED:
            service->connection_handle = BLE_CONN_HANDLE_INVALID;
            NRF_LOG_INFO("Disconnected (conn_handle: %d)", p_ble_evt->evt.gap_evt.conn_handle);
            break;

        case BLE_GAP_EVT_CONNECTED:
            NRF_LOG_INFO("Connected (conn_handle: %d)", p_ble_evt->evt.gap_evt.conn_handle);
            service->connection_handle = p_ble_evt->evt.gap_evt.conn_handle;
            
            err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
            APP_ERROR_CHECK(err_code);

            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_conn_handle);
            APP_ERROR_CHECK(err_code);

            rgb_t rgb;
            hsv2rgb((hsv_t*)(&g_data.current.hsv), &rgb);
            uint8_t rgb_state_value[ESTC_CHAR_LED_RGB_STATE_LEN] = { 0 };
            rgb_state_value[0] = (uint8_t)(rgb.r * 255 / 100);
            rgb_state_value[1] = (uint8_t)(rgb.g * 255 / 100);
            rgb_state_value[2] = (uint8_t)(rgb.b * 255 / 100);
            rgb_state_value[3] = (uint8_t)(g_data.current.state);
            estc_ble_set_value(service, rgb_state_value, ESTC_CHAR_LED_RGB_STATE_LEN);
            break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        {
            NRF_LOG_INFO("PHY update request (conn_handle: %d)", p_ble_evt->evt.gap_evt.conn_handle);
            ble_gap_phys_t const phys =
            {
                .rx_phys = BLE_GAP_PHY_AUTO,
                .tx_phys = BLE_GAP_PHY_AUTO,
            };
            err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            APP_ERROR_CHECK(err_code);
        } break;

        case BLE_GATTC_EVT_TIMEOUT:
            // Disconnect on GATT Client timeout event.
            NRF_LOG_INFO("GATT Client Timeout (conn_handle: %d)", p_ble_evt->evt.gattc_evt.conn_handle);
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_TIMEOUT:
            // Disconnect on GATT Server timeout event.
            NRF_LOG_INFO("GATT Server Timeout (conn_handle: %d)", p_ble_evt->evt.gatts_evt.conn_handle);
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;
        
        case BLE_GATTS_EVT_WRITE:
            blinky_ble_on_write(p_ble_evt, data);
            break;

        default:
            break;
    }
}


/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    ret_code_t err_code;

    err_code = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err_code);

    // Configure the BLE stack using the default settings.
    // Fetch the start address of the application RAM.
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    APP_ERROR_CHECK(err_code);

    // Enable BLE stack.
    err_code = nrf_sdh_ble_enable(&ram_start);
    APP_ERROR_CHECK(err_code);

    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, (void*)&g_data);
}

/**@brief Function for initializing the Advertising functionality.
 */
static void advertising_init(void)
{
    ret_code_t             err_code;
    ble_advertising_init_t init;

    memset(&init, 0, sizeof(init));

    init.advdata.name_type               = BLE_ADVDATA_FULL_NAME;
    init.advdata.flags                   = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;

    init.advdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    init.advdata.uuids_complete.p_uuids  = m_adv_uuids;

    init.config.ble_adv_fast_enabled  = true;
    init.config.ble_adv_fast_interval = APP_ADV_INTERVAL;
    init.config.ble_adv_fast_timeout  = APP_ADV_DURATION;

    init.evt_handler = on_adv_evt;

    err_code = ble_advertising_init(&m_advertising, &init);
    APP_ERROR_CHECK(err_code);

    ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);
}

/**@brief Function for initializing the nrf log module.
 */
static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}

/**@brief Function for initializing power management.
 */
static void power_management_init(void)
{
    ret_code_t err_code;
    err_code = nrf_pwr_mgmt_init();
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for handling the idle state (main loop).
 *
 * @details If there is no pending log operation, then sleep until next the next event occurs.
 */
static void idle_state_handle(void)
{
    if (NRF_LOG_PROCESS() == false)
    {
        nrf_pwr_mgmt_run();
    }
	LOG_BACKEND_USB_PROCESS();
}

void app_sys_event_handler(uint32_t sys_evt)
{
	switch (sys_evt)
	{
		case NRF_EVT_FLASH_OPERATION_SUCCESS:
			NRF_LOG_INFO("MAIN: NRF_EVT_FLASH_OPERATION_SUCCESS");
			break;

		case NRF_EVT_FLASH_OPERATION_ERROR:
			NRF_LOG_INFO("MAIN: NRF_EVT_FLASH_OPERATION_ERROR");
			break;
	}
}

/**@brief Function for starting advertising.
 */
static void advertising_start(void)
{
    ret_code_t err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
    APP_ERROR_CHECK(err_code);
}

static void blinky_set_led_rgb(rgb_t* rgb, bool state)
{
    ASSERT(NULL != rgb);
    if(state)
    {
        blinky_led_pwm_set(BLINKY_LED_R, rgb->r);
        blinky_led_pwm_set(BLINKY_LED_G, rgb->g);
        blinky_led_pwm_set(BLINKY_LED_B, rgb->b);
    }
    else
    {
        blinky_led_pwm_set(BLINKY_LED_R, 0);
        blinky_led_pwm_set(BLINKY_LED_G, 0);
        blinky_led_pwm_set(BLINKY_LED_B, 0);
    }
}

static bool are_hsv_equal(hsv_t* first, hsv_t* second)
{
    ASSERT(NULL != first);
    ASSERT(NULL != second);

    if ((abs(first->h - second->h) < 1.f) &&
        (abs(first->s - second->s) < 1.f) &&
        (abs(first->v - second->v) < 1.f))
    {
        return true;
    }
    return false;
}

static bool blinky_are_current_equal(color_data_t* first, color_data_t* second)
{
    ASSERT(NULL != first);
    ASSERT(NULL != second);
    
    if (!are_hsv_equal(&(first->hsv), &(second->hsv)))
    {
        return false;
    }

    for (uint32_t i = 0; i < BLINKY_SAVED_COLOR_CNT; ++i)
    {
        if( 0 != strcmp(first->hsvname_pair_array[i].name, second->hsvname_pair_array[i].name) ||
            !are_hsv_equal(&(first->hsvname_pair_array[i].hsv), &(second->hsvname_pair_array[i].hsv)))
        {
            return false;
        }
    }

    if (first->state != second->state)
    {
        return false;
    }

    return true;
}

static void blinky_save_data(data_t* data)
{
    NRF_LOG_INFO("MAIN: blinky_save_data");
    ASSERT(NULL != data);

    if (blinky_are_current_equal((color_data_t*)&(data->current), &(data->saved)))
    {
        NRF_LOG_INFO("MAIN: blinky_save_data: Nothing to save");
        return;
    }

    bool result = blinky_nvmc_write_data((uint32_t*)&(data->current), sizeof(data->current));
    NRF_LOG_INFO("MAIN: blinky_save_data: blinky_nvmc_write_data result=%d", result);
    if(result)
    {
        //while (!blinky_nvmc_write_done_check())
        //{}
    }
    NRF_LOG_INFO("MAIN: blinky_save_data: writing complete");
    memcpy(&(g_data.saved), (void*)&(g_data.current), sizeof(g_data.saved));
}

static void blinky_state_to_led(state_t state)
{
    switch(state)
    {
        case T_VIEW:
            blinky_led_soft_off(BLINKY_LED_0);
            break;
        case T_EDIT_HUE:
            blinky_led_soft_on(BLINKY_LED_0, BLINKY_STATE_SLOW_BLINK_MS);
            break;
        case T_EDIT_SATURATION:
            blinky_led_soft_on(BLINKY_LED_0, BLINKY_STATE_FAST_BLINK_MS);
            break;
        case T_EDIT_BRIGHTNESS:
            blinky_led_soft_off(BLINKY_LED_0);
            blinky_led_pwm_set(BLINKY_LED_0, 100);
            break;
        case T_COUNT: // fall-through
        default:
            ASSERT(false);
            break;
    }
}

STATIC_ASSERT(sizeof(g_data.saved) % sizeof(uint32_t) == 0, "struct must be aligned to 32 bit word");

static char* blinky_state_to_str(state_t state)
{
    switch(state)
    {
        case T_VIEW:            return "VIEW";
        case T_EDIT_HUE:        return "EDIT HUE";
        case T_EDIT_SATURATION: return "EDIT SATURATION";
        case T_EDIT_BRIGHTNESS: return "EDIT BRIGHTNESS";
        case T_COUNT:           // fall-through
        default:                return "UNKNOWN STATE";
    }
}

void blinky_on_button_hold(void * p_context)
{
    NRF_LOG_INFO("MAIN: blinky_on_button_hold");
    UNUSED_VARIABLE(p_context);

    NRF_LOG_INFO("MAIN: blinky_on_button_hold: BLINKY_VELOCITY_MS = %u", BLINKY_VELOCITY_MS);

    ret_code_t res = app_timer_start(g_timer_move, APP_TIMER_TICKS(BLINKY_VELOCITY_MS), NULL);
    ASSERT(NRF_SUCCESS == res);
}

void blinky_on_button_release(void * p_context)
{
    NRF_LOG_INFO("MAIN: blinky_on_button_release");
    UNUSED_VARIABLE(p_context);

    ret_code_t res = app_timer_stop(g_timer_move);
    ASSERT(NRF_SUCCESS == res);

    NRF_LOG_INFO("MAIN: blinky_on_button_release: HSV: %d %d %d", g_data.current.hsv.h, g_data.current.hsv.s, g_data.current.hsv.v);
}

void blinky_on_button_multi_click(void * p_context)
{
    NRF_LOG_INFO("MAIN: blinky_on_button_multi_click");
    ASSERT(NULL!= p_context);
    uint32_t click_cnt = (uint32_t)p_context;

    switch (click_cnt)
    {
        case 1:
        {
            NRF_LOG_INFO("MAIN: blinky_on_button_multi_click: Single click handling...");

                ASSERT(sizeof(g_data.saved) == sizeof(g_data.current))
    blinky_nvmc_init(sizeof(g_data.saved));
    
    uint32_t read = blinky_nvmc_read_last_data((uint32_t*)&(g_data.saved), sizeof(g_data.saved));
    NRF_LOG_INFO("MAIN: blinky_on_button_multi_click: blinky_nvmc_get_last_data read %u bytes", read);
    if (read > 0)
    {
        NRF_LOG_INFO("MAIN: saved.hsv.h=" NRF_LOG_FLOAT_MARKER, NRF_LOG_FLOAT(g_data.saved.hsv.h));
        NRF_LOG_INFO("MAIN: saved.hsv.v=" NRF_LOG_FLOAT_MARKER, NRF_LOG_FLOAT(g_data.saved.hsv.v));
        NRF_LOG_INFO("MAIN: saved.hsv.s=" NRF_LOG_FLOAT_MARKER, NRF_LOG_FLOAT(g_data.saved.hsv.s));
        for (uint32_t i = 0; i < BLINKY_SAVED_COLOR_CNT; ++i)
        {
            NRF_LOG_INFO("MAIN: hsvname_pair.name=%s", g_data.saved.hsvname_pair_array[i].name);
            NRF_LOG_INFO("MAIN: hsvname_pair.hsv.h=" NRF_LOG_FLOAT_MARKER, NRF_LOG_FLOAT(g_data.saved.hsvname_pair_array[i].hsv.h));
            NRF_LOG_INFO("MAIN: hsvname_pair.hsv.v=" NRF_LOG_FLOAT_MARKER, NRF_LOG_FLOAT(g_data.saved.hsvname_pair_array[i].hsv.v));
            NRF_LOG_INFO("MAIN: hsvname_pair.hsv.s=" NRF_LOG_FLOAT_MARKER, NRF_LOG_FLOAT(g_data.saved.hsvname_pair_array[i].hsv.s));
        }
        memcpy((void*)&(g_data.current), &(g_data.saved), sizeof(g_data.saved));
    }

    // Color init defaults
    NRF_LOG_INFO("MAIN: Color init defaults");
    rgb_t rgb = {0.f};
    hsv2rgb((hsv_t*)&(g_data.current.hsv), &rgb);
    blinky_set_led_rgb(&rgb, g_data.current.state);


            break;
        }
        case 2:
            NRF_LOG_INFO("MAIN: blinky_on_button_multi_click: Double click handling...");
            NRF_LOG_INFO("MAIN: blinky_on_button_multi_click: old state: %s", blinky_state_to_str(g_data.state));
            g_data.state++;
            g_data.state %= T_COUNT;
            blinky_state_to_led(g_data.state);
            NRF_LOG_INFO("MAIN: blinky_on_button_multi_click: new state: %s", blinky_state_to_str(g_data.state));
            if(g_data.state == T_VIEW)
            {
                blinky_save_data(&g_data);
            }
            break;
        case 3:
            NRF_LOG_INFO("MAIN: blinky_on_button_multi_click: Triple click handling...");
            break;
        default:
            break;
    }
}

static void blinky_360_run(volatile float* value)
{
    ASSERT(NULL != value);
    *value += 1.f;
    if (*value > 360.f)
    {
        *value = 0.f;
    }
}

static void blinky_100_run(volatile float* value, volatile bool* up)
{
    ASSERT(NULL != value);
    ASSERT(NULL != up);
    if (*up)
    {
        *value += 1.f;
        if( *value > 100.f)
        {
            *value = 100.f;
            *up = false;
        }
    }
    else
    {
        *value -= 1.f;
        if( *value < 0.f)
        {
            *value = 0.f;
            *up = true;
        }
    }
}

void app_timer_move_handler(void * p_context)
{
    switch(g_data.state)
    {
        case T_VIEW:
            break;
    
        case T_EDIT_HUE:
            blinky_360_run(&(g_data.current.hsv.h));
            break;

        case T_EDIT_SATURATION:
            blinky_100_run(&(g_data.current.hsv.s), &(g_data.move_s_up));
            break;

        case T_EDIT_BRIGHTNESS:
            blinky_100_run(&(g_data.current.hsv.v), &(g_data.move_v_up));
            break;

        case T_COUNT: // fall-through
        default:
            ASSERT(false);
            break;
    }
 
    rgb_t rgb = { 0.f };
    hsv2rgb((hsv_t*)&(g_data.current.hsv), &rgb);
    blinky_set_led_rgb(&rgb, g_data.current.state);

    uint8_t rgb_state_value[ESTC_CHAR_LED_RGB_STATE_LEN] = { 0 };
    rgb_state_value[0] = (uint8_t)(rgb.r * 255 / 100);
    rgb_state_value[1] = (uint8_t)(rgb.g * 255 / 100);
    rgb_state_value[2] = (uint8_t)(rgb.b * 255 / 100);
    rgb_state_value[3] = (uint8_t)(g_data.current.state);

    estc_ble_set_value(&(g_data.estc_ble_service), rgb_state_value, ESTC_CHAR_LED_RGB_STATE_LEN);
}


/**@brief Function for application main entry.
 */
int main(void)
{
    memset((void*)g_data.current.hsvname_pair_array, 0, sizeof(g_data.current.hsvname_pair_array));
    memset((void*)g_data.saved.hsvname_pair_array, 0, sizeof(g_data.saved.hsvname_pair_array));

    // Initialize.
    log_init();
    timers_init();
    //buttons_leds_init();

    ret_code_t res = app_timer_create(&g_timer_move, APP_TIMER_MODE_REPEATED, app_timer_move_handler);
    ASSERT(NRF_SUCCESS == res);

    /* Leds init */
    NRF_LOG_INFO("MAIN: Leds init");
    blinky_led_soft_init();

    /* HACK to turn off leds that is a result of ASSERT in USB CDC ACM module during init */
    blinky_led_pwm_set(BLINKY_LED_0, 0);
    blinky_led_pwm_set(BLINKY_LED_1, 0);
    blinky_led_pwm_set(BLINKY_LED_2, 0);
    blinky_led_pwm_set(BLINKY_LED_3, 0);

    /* Buttons init */
    NRF_LOG_INFO("MAIN: Buttons init");
    blinky_btns_init(blinky_on_button_hold, blinky_on_button_release, blinky_on_button_multi_click);

    // NVMC init 
/*    NRF_LOG_INFO("MAIN: NVMC init");
    ASSERT(sizeof(g_data.saved) == sizeof(g_data.current))
    blinky_nvmc_init(sizeof(g_data.saved));
    
    uint32_t read = blinky_nvmc_read_last_data((uint32_t*)&(g_data.saved), sizeof(g_data.saved));
    NRF_LOG_INFO("MAIN: blinky_on_button_multi_click: blinky_nvmc_get_last_data read %u bytes", read);
    if (read > 0)
    {
        NRF_LOG_INFO("MAIN: saved.hsv.h=" NRF_LOG_FLOAT_MARKER, NRF_LOG_FLOAT(g_data.saved.hsv.h));
        NRF_LOG_INFO("MAIN: saved.hsv.v=" NRF_LOG_FLOAT_MARKER, NRF_LOG_FLOAT(g_data.saved.hsv.v));
        NRF_LOG_INFO("MAIN: saved.hsv.s=" NRF_LOG_FLOAT_MARKER, NRF_LOG_FLOAT(g_data.saved.hsv.s));
        for (uint32_t i = 0; i < BLINKY_SAVED_COLOR_CNT; ++i)
        {
            NRF_LOG_INFO("MAIN: hsvname_pair.name=%s", g_data.saved.hsvname_pair_array[i].name);
            NRF_LOG_INFO("MAIN: hsvname_pair.hsv.h=" NRF_LOG_FLOAT_MARKER, NRF_LOG_FLOAT(g_data.saved.hsvname_pair_array[i].hsv.h));
            NRF_LOG_INFO("MAIN: hsvname_pair.hsv.v=" NRF_LOG_FLOAT_MARKER, NRF_LOG_FLOAT(g_data.saved.hsvname_pair_array[i].hsv.v));
            NRF_LOG_INFO("MAIN: hsvname_pair.hsv.s=" NRF_LOG_FLOAT_MARKER, NRF_LOG_FLOAT(g_data.saved.hsvname_pair_array[i].hsv.s));
        }
        memcpy((void*)&(g_data.current), &(g_data.saved), sizeof(g_data.saved));
    }
*/
    // Color init defaults
    NRF_LOG_INFO("MAIN: Color init defaults");
    rgb_t rgb = {0.f};
    hsv2rgb((hsv_t*)&(g_data.current.hsv), &rgb);
    blinky_set_led_rgb(&rgb, g_data.current.state);

    uint8_t rgb_state_value[ESTC_CHAR_LED_RGB_STATE_LEN] = { 0 };
    rgb_state_value[0] = (uint8_t)(rgb.r * 255 / 100);
    rgb_state_value[1] = (uint8_t)(rgb.g * 255 / 100);
    rgb_state_value[2] = (uint8_t)(rgb.b * 255 / 100);
    rgb_state_value[3] = (uint8_t)(g_data.current.state);
    estc_ble_set_value(&(g_data.estc_ble_service), rgb_state_value, ESTC_CHAR_LED_RGB_STATE_LEN);

    power_management_init();
    ble_stack_init();
    gap_params_init();
    gatt_init();
    services_init();
    advertising_init();
    conn_params_init();

    // Start execution.
    NRF_LOG_INFO("ESTC GATT server example started");
    application_timers_start();

    advertising_start();

    // Enter main loop.
    for (;;)
    {
        if (g_data.need_save)
        {
            blinky_save_data(&g_data);
            g_data.need_save = false;
        }
        idle_state_handle();
    }
}


/**
 * @}
 */

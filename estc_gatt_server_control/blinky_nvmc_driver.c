#include "nrfx_nvmc.h"

#include "blinky_nvmc_driver.h"
#include "blinky_log.h"

volatile bool g_wait_for_erase = false;
volatile bool g_wait_for_write = false;

void fstorage_evt_handler(nrf_fstorage_evt_t * p_evt);
NRF_FSTORAGE_DEF(nrf_fstorage_t g_storage) =
{
    .evt_handler = fstorage_evt_handler,
    .start_addr = BLINKY_PAGE0_BEGIN,
    .end_addr   = BLINKY_PAGE0_BEGIN + 3 * BLINKY_PAGE_SIZE,
};

void fstorage_evt_handler(nrf_fstorage_evt_t * p_evt)
{
    NRF_LOG_INFO("fstorage_evt_handler");
    switch(p_evt->id)
    {
        case NRF_FSTORAGE_EVT_READ_RESULT:
        NRF_LOG_INFO("NRF_FSTORAGE_EVT_READ_RESULT");
        break;
        case NRF_FSTORAGE_EVT_WRITE_RESULT:
        NRF_LOG_INFO("NRF_FSTORAGE_EVT_WRITE_RESULT");
        g_wait_for_write = false;
        break;
        case NRF_FSTORAGE_EVT_ERASE_RESULT:
        NRF_LOG_INFO("NRF_FSTORAGE_EVT_ERASE_RESULT");
        g_wait_for_erase = false;
        break;
        default:
        NRF_LOG_INFO("NRF_FSTORAGE_EVT_UNKNOWN");
        break;
    }
    NRF_LOG_INFO("Result of the operation. result:%d", p_evt->result);
    NRF_LOG_INFO("Address at which the operation was performed. addr:0x%x",  p_evt->addr);
    NRF_LOG_INFO("Buffer written to flash. p_src:0x%x", p_evt->p_src);
    NRF_LOG_INFO("Length of the operation. len:%u",  p_evt->len);
    NRF_LOG_INFO("User-defined parameter passed to the event handler. p_param:0x%x", p_evt->p_param);
}

void blinky_driver_init()
{
    NRF_LOG_INFO("NVMC DRV: blinky_driver_init");
    nrf_fstorage_init(&g_storage, &nrf_fstorage_sd, NULL);
}

void blinky_read_block(uint32_t* addr, uint32_t* payload, uint32_t word_cnt)
{
    NRF_LOG_INFO("NVMC DRV: blinky_read_block: read addr = 0x%x, payload addr = 0x%x, count = %u", (uint32_t)addr, (uint32_t)payload, word_cnt);

    ASSERT(NULL != addr);
    ASSERT(NULL != payload);
    ASSERT(word_cnt > 0);

    memcpy(payload, addr, word_cnt * BLINKY_WORD_SIZE);
}

bool blinky_block_writable_check(uint32_t* addr, uint32_t* payload, uint32_t word_cnt)
{
    return true;
    NRF_LOG_INFO("NVMC DRV: blinky_block_writable_check: addr=0x%x, payload=0x%x, word_cnt=%u", addr, payload, word_cnt);
    
    ASSERT(NULL != addr);
    ASSERT(NULL != payload);
    ASSERT(0 != word_cnt);

    for (uint32_t i = 0; i < word_cnt; ++i)
    {   
        
        if (!nrfx_nvmc_word_writable_check((uint32_t)addr, *payload))
        {
            //NRF_LOG_INFO("NVMC DRV: blinky_block_writable_check: payload data, addr=0x%x, value=0x%x is NOT writable", (uint32_t)addr, *payload);
            return false;
        }

        addr++;
        payload++;
    }

    return true;
 }

void blinky_block_write(uint32_t* addr, uint32_t* payload, uint32_t word_cnt)
{
    NRF_LOG_INFO("NVMC DRV: blinky_block_write: addr=0x%x, payload=0x%x, word_cnt=%u", addr, payload, word_cnt);

    ASSERT(NULL != addr);
    ASSERT(NULL != payload);
    ASSERT(word_cnt > 0);

    nrf_fstorage_write(&g_storage, (uint32_t)addr, payload, word_cnt * BLINKY_WORD_SIZE, NULL);
    NRF_LOG_INFO("NVMC DRV: wait for writing complete");
    g_wait_for_write = true;
    while(g_wait_for_write)
    {}
    NRF_LOG_INFO("NVMC DRV: writing complete");
}

nrfx_err_t blinky_nvmc_erase_page(uint32_t* page_start_addr)
{
    NRF_LOG_INFO("NVMC DRV: blinky_nvmc_erase_page: page_start_addr=0x%x", page_start_addr);

    nrf_fstorage_erase(&g_storage, (uint32_t)page_start_addr, 1, NULL);
    NRF_LOG_INFO("NVMC DRV: wait for erasing complete");
    g_wait_for_erase = true;
    while(g_wait_for_erase)
    {}
    NRF_LOG_INFO("NVMC DRV: erasing complete");
    return NRFX_SUCCESS;
}

bool blinky_driver_write_done_check()
{
    //return !nrf_fstorage_is_busy(&g_storage);
    return true;
}
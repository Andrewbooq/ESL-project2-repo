#include "nrfx_nvmc.h"

#include "blinky_nvmc.h"
#include "blinky_nvmc_driver.h"
#include "blinky_log.h"
#include "blinky_types.h"

#define BLINKY_EMPTY_VALUE              0xFFFFFFFF

static nvmc_t g_nvmc = 
{
    .need_to_erase_page0 = false,
    .last_addr = NULL,
    .writable_block_size = 0
};

static bool blinky_find_last_addr(uint32_t* result)
{
    uint32_t* addr = (uint32_t*)BLINKY_PAGE0_BEGIN;
    uint32_t* last_addr = 0;

    for (int i = 0 ;  ; ++i)
    {
        NRF_LOG_INFO("NVMC: blinky_find_last_addr: check addr=0x%x", addr);
        header_t header = {0};
        blinky_read_block(addr, &(header.block_size), sizeof(header)/BLINKY_WORD_SIZE);
        NRF_LOG_INFO("NVMC: blinky_find_last_addr: header=%u", header.block_size);
        if ((header.block_size == BLINKY_EMPTY_VALUE))
        {
            if (i == 0)
            {
                // there is no header in memory
                NRF_LOG_INFO("NVMC: blinky_find_last_addr: empty memory");
                return false;
            }
            // the previous block was the last correct
            NRF_LOG_INFO("NVMC: blinky_find_last_addr: the previous block was the last correct");
            *result = (uint32_t)last_addr;
            return true;
        }

        last_addr = addr;
        addr += sizeof(header)/BLINKY_WORD_SIZE;            /* header */
        addr += (header.block_size / BLINKY_WORD_SIZE);     /* data */

        if ((uint32_t)addr > BLINKY_PAGE1_BEGIN)
        {
            NRF_LOG_INFO("NVMC: blinky_find_last_addr: end of page, mark page 0 to erase");
            g_nvmc.need_to_erase_page0 = true;
            break;
        }
    }
    return false;
}

void blinky_nvmc_init(uint32_t writable_block_size)
{
    ASSERT(writable_block_size > 0);
    
    blinky_driver_init();
    
    g_nvmc.writable_block_size = writable_block_size;

    NRF_LOG_INFO("NVMC: blinky_nvmc_init: initialize g_nvmc.last_addr to first byte of first page");
    NRF_LOG_INFO("NVMC: blinky_nvmc_init: BLINKY_BOOTLOADER_START_ADDR=0x%x", BLINKY_BOOTLOADER_START_ADDR);
    NRF_LOG_INFO("NVMC: blinky_nvmc_init: NRF_DFU_APP_DATA_AREA_SIZE=0x%x", NRF_DFU_APP_DATA_AREA_SIZE);
    NRF_LOG_INFO("NVMC: blinky_nvmc_init: BLINKY_APP_AREA_BEGIN_ADDR=0x%x", BLINKY_APP_AREA_BEGIN_ADDR);
    NRF_LOG_INFO("NVMC: blinky_nvmc_init: g_nvmc.writable_block_size=%u", g_nvmc.writable_block_size);
    g_nvmc.last_addr = (uint32_t*)BLINKY_APP_AREA_BEGIN_ADDR;
    NRF_LOG_INFO("NVMC: blinky_nvmc_init: g_nvmc.last_addr=0x%x", g_nvmc.last_addr);
}

uint32_t blinky_nvmc_read_last_data(uint32_t* data, uint32_t size)
{
    NRF_LOG_INFO("NVMC: blinky_nvmc_read_last_data: data addr=0x%x, size=%u", (uint32_t)data, size);

    ASSERT(NULL != data);
    ASSERT(size == g_nvmc.writable_block_size);
    
    uint32_t result = 0;
    bool found = blinky_find_last_addr(&result);

    if(!found)
    {
        NRF_LOG_INFO("NVMC: blinky_nvmc_read_last_data: wrong data in memory");
        return 0;
    }

    uint32_t* addr = (uint32_t*)result;

    header_t header = { 0 };
    uint32_t header_word_cnt = sizeof(header)/BLINKY_WORD_SIZE;

    blinky_read_block(addr, (uint32_t*)&header, header_word_cnt);
    NRF_LOG_INFO("NVMC: blinky_nvmc_read_last_data: blinky_read_header=%u", header.block_size);

    if(header.block_size != size)
    {
        NRF_LOG_INFO("NVMC: blinky_nvmc_read_last_data: invalid header value");
        // Invalid value, need to clean the page
        // Reading function: don't write or erase here
        // So, just mark the page for erasing
        g_nvmc.need_to_erase_page0 = true;
        return 0;
    }
    g_nvmc.last_addr = addr;
    
    /* Header looks good, continue */
    g_nvmc.last_addr += header_word_cnt;
    
    /* Read data, skeep header */
    uint32_t data_word_cnt = header.block_size / BLINKY_WORD_SIZE;
    blinky_read_block(g_nvmc.last_addr, data, data_word_cnt);
    g_nvmc.last_addr += data_word_cnt;

    return header.block_size;
}

bool blinky_nvmc_write_data(uint32_t* data, uint32_t size)
{
    NRF_LOG_INFO("NVMC: blinky_nvmc_write_data: data=0x%x, size=%u", (uint32_t)data, size);

    ASSERT(NULL != data);
    ASSERT(size == g_nvmc.writable_block_size);

    if (g_nvmc.need_to_erase_page0)
    {
        NRF_LOG_INFO("NVMC: blinky_nvmc_write_data: Page number 0 is marked to erase, clean it");

        g_nvmc.last_addr = (uint32_t*)BLINKY_PAGE0_BEGIN;
        nrfx_err_t resx = blinky_nvmc_erase_page((uint32_t*)BLINKY_PAGE0_BEGIN);
        if (NRFX_SUCCESS != resx)
        {
            NRF_LOG_INFO("NVMC: blinky_nvmc_write_data: Cannot erase page 0, nrfx_err_t=%d", resx);
            return false;
        }
        g_nvmc.need_to_erase_page0 = false;
    }

    /* Check if the data fit free space */
    uint32_t free_space = BLINKY_PAGE1_BEGIN - (uint32_t)g_nvmc.last_addr;
    NRF_LOG_INFO("NVMC: blinky_nvmc_write_data: free_space=%u", free_space);
    if (free_space < (size + sizeof(header_t)))
    {
        NRF_LOG_INFO("NVMC: blinky_nvmc_write_data: Not enought free space, erase page 0");
        
        g_nvmc.last_addr = (uint32_t*)BLINKY_PAGE0_BEGIN;
        nrfx_err_t resx = blinky_nvmc_erase_page((uint32_t*)BLINKY_PAGE0_BEGIN);
        if (NRFX_SUCCESS != resx)
        {
            NRF_LOG_INFO("NVMC: blinky_nvmc_write_data: Cannot erase page 0, nrfx_err_t=%d", resx);
            return false;
        }
    }

    /* Header */
    header_t header = { 0 };
    header.block_size = size;
    uint32_t header_word_cnt = sizeof(header)/BLINKY_WORD_SIZE;

    /* Data */
    uint32_t data_word_cnt = size / BLINKY_WORD_SIZE;

    /* Write Header */
    NRF_LOG_INFO("NVMC: blinky_nvmc_write_data: Able to write, writing");
    blinky_block_write(g_nvmc.last_addr, (uint32_t*)&header, header_word_cnt);
    g_nvmc.last_addr += header_word_cnt;

    /* Write Data */
    blinky_block_write(g_nvmc.last_addr, data, data_word_cnt);
 
    g_nvmc.last_addr += data_word_cnt;

    NRF_LOG_INFO("NVMC: blinky_nvmc_write_data: data pushed for writing");
    return true;
}

bool blinky_nvmc_write_done_check(void)
{
    return blinky_driver_write_done_check();
}
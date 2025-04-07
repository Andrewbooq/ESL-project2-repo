#ifndef BLINKY_NVMC_DRIVER_H__
#define BLINKY_NVMC_DRIVER_H__

#include "nrf_dfu_types.h"
#include "nrf_bootloader_info.h"
#include "nrf_fstorage.h"
#include "nrf_fstorage_sd.h"

#define BLINKY_WORD_SIZE                (sizeof(uint32_t))


#define BLINKY_BOOTLOADER_START_ADDR    (0xE0000)
#define BLINKY_PAGE_SIZE                (0x1000)
#define BLINKY_APP_AREA_BEGIN_ADDR      (BLINKY_BOOTLOADER_START_ADDR - NRF_DFU_APP_DATA_AREA_SIZE)
#define BLINKY_PAGE0_BEGIN              BLINKY_APP_AREA_BEGIN_ADDR
#define BLINKY_PAGE1_BEGIN              (BLINKY_PAGE0_BEGIN + BLINKY_PAGE_SIZE)

/* Address to write to. Must be word-aligned. */
void blinky_driver_init();
void blinky_read_block(uint32_t* addr, uint32_t* payload, uint32_t word_cnt);
bool blinky_block_writable_check(uint32_t* addr, uint32_t* payload, uint32_t word_cnt);
void blinky_block_write(uint32_t* addr, uint32_t* payload, uint32_t word_cnt);
nrfx_err_t blinky_nvmc_erase_page(uint32_t* page_start_addr);
bool blinky_driver_write_done_check();
#endif /* BLINKY_NVMC_DRIVER_H__ */
/*
 * raw_sd_diskio.c
 *
 *  Created on: Feb 21, 2026
 *      Author: YOOCHAN (TddKim0712)
 *
 *      SKKU Aerospace Study Club ARES
 *      RAW sd logger for Avionics, only educational purpose
 *
 */


#include "raw_sd_diskio.h"

static volatile uint32_t g_tx_done = 0;
static volatile uint32_t g_rx_done = 0;

void BSP_SD_WriteCpltCallback(void)
{
    g_tx_done = 1;
}

void BSP_SD_ReadCpltCallback(void)
{
    g_rx_done = 1;
}

static int wait_card_ready(uint32_t timeout)
{
    uint32_t t0 = HAL_GetTick();
    while((HAL_GetTick() - t0) < timeout)
    {
        if(BSP_SD_GetCardState() == SD_TRANSFER_OK)
            return 0;
    }
    return -1;
}

int raw_sd_init(void)
{
    if(BSP_SD_Init() != MSD_OK)
        return -1;
    return 0;
}

int raw_sd_write(uint32_t *buf, uint32_t lba, uint32_t sector_count)
{
	// card ready time should be changed after debugging
    if(wait_card_ready(3000) < 0)
        return -1;

    g_tx_done = 0;

    if(BSP_SD_WriteBlocks_DMA(buf, lba, sector_count) != MSD_OK)
        return -2;

    while(!g_tx_done);

    if(wait_card_ready(3000) < 0)
        return -3;

    return 0;
}

int raw_sd_read(uint32_t *buf, uint32_t lba, uint32_t sector_count)
{
    if(wait_card_ready(3000) < 0)
        return -1;

    g_rx_done = 0;

    if(BSP_SD_ReadBlocks_DMA(buf, lba, sector_count) != MSD_OK)
        return -2;

    while(!g_rx_done);

    if(wait_card_ready(3000) < 0)
        return -3;

    return 0;
}


/*
 * raw_sd_diskio.h
 *
 *  Created on: Feb 21, 2026
 *      Author: YOOCHAN KIM (TddKim0712)
 *      SKKU Aerospace Study Club ARES
 *      RAW sd logger for Avionics, only educational purpose
 */
#ifndef RAW_SD_DISKIO_H
#define RAW_SD_DISKIO_H

#include "bsp_driver_sd.h"
#include "stm32f4xx_hal.h"

#define RAW_SECTOR_SIZE 512
#define RAW_EXTENT_SIZE 4096
#define RAW_EXTENT_SECTORS (RAW_EXTENT_SIZE / RAW_SECTOR_SIZE)

int raw_sd_init(void);

int raw_sd_write(uint32_t *buf, uint32_t lba, uint32_t sector_count);

int raw_sd_read(uint32_t *buf, uint32_t lba, uint32_t sector_count);

#endif

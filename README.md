# NUCLEO-F411RE-SDIO-DMA

SDIO + DMA based SD card logger on STM32 NUCLEO-F411RE.

This project implements a high-speed and stable SD card logging system
using the STM32 SDIO peripheral with DMA support.

## Features

- SDIO interface (SDHC)
- 4-bit wide data bus mode
- DMA-based block read/write
- FATFS integration
- Tested on NUCLEO-F411RE hardware
- Designed for continuous data logging  
  (currently time + dummy data only, no real-time sensors)

## Implementation Notes

- The SD card module must support **SDIO bus**, not SPI  
  (check whether CMD and DAT pins are exposed)
- SD card initialization starts in **1-bit mode** and switches to **4-bit wide mode**
- SDIO clock configuration is tuned based on personal stability and performance tests
- DMA is used for data transfer to minimize CPU load
- Based on STM32CubeIDE generated code  
  (status handling functions and variables are user-modified)
- Different MCU/MPU targets may require user-side code adjustments

## Hardware Configuration

- **Board**: STM32 NUCLEO-F411RE
- **SD Interface**: SDIO (4-bit wide)
- **Clock**:
  - SDIO clock divider configured for safe initialization (PLLCLK = SDIOCLK = 48Mhz)
  - Increased after card initialization for data transfer    SDIO_CK = SDIOCLK / (ClockDiv + 2), ClockDiv = 0 --> 2
    <img width="1564" height="510" alt="image" src="https://github.com/user-attachments/assets/f688a7f1-4236-4b88-829c-35359ab21861" />

- **DMA**:
  - SDIO RX/TX connected to DMA controller
  - Interrupt-driven transfer completion handling
- **NVIC**:
  - SDIO global interrupt enabled
  - DMA stream interrupts enabled
- **Pin Mapping**:
  - CMD, CLK, DAT[0:3] mapped according to STM32F411 SDIO pinout
  - External pull-ups required on CMD and DAT lines (SD card side)
 
    <img width="706" height="630" alt="image" src="https://github.com/user-attachments/assets/b66d57c6-3538-493e-9902-cd5851be9696" />


## License

This project is released under the MIT License.

It includes STM32 HAL and BSP drivers provided by STMicroelectronics,
which are licensed under ST’s BSD-style license.

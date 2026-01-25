# NUCLEO-F411RE-SDIO-DMA

SDIO + DMA based SD card logger on STM32 NUCLEO-F411RE.

NUCLEO-F411RE의 data brief: https://www.st.com/resource/en/data_brief/nucleo-f411re.pdf   
내장 MCU datasheet: https://www.st.com/resource/en/datasheet/stm32f411re.pdf   

This project implements a high-speed and stable SD card logging system
using the STM32 SDIO peripheral with DMA support.


## Features

- SDIO interface (SDHC, 32GB sd card)
- 4-bit wide data bus mode
- DMA-based block read/write
- FAT32, no exFAT or NTFS
- Tested on NUCLEO-F411RE hardware
- Designed for continuous data logging  
  (currently time + dummy data only, no real-time sensors yet)

  현재는 매 루프마다 fwrite로 systick시간, seq 순서, 오류여부를 기록함.
  

## Implementation Notes

- The SD card module must support **SDIO bus**, not SPI  
  (check whether CMD and DAT pins are exposed)
- SD card initialization starts in **1-bit mode** and switches to **4-bit wide mode**
- SDIO clock configuration is tuned based on personal stability and performance tests
- DMA is used for data transfer to minimize CPU load
- Based on STM32CubeIDE generated code  
  (status handling functions and variables are user-modified)
- Different MCU/MPU targets may require user-side code adjustments

### System Logic
0. fatfs.c -> MX_FATFS_INIT 에서 sd_diskio.c 의 SD_Driver로 link  
   (MX_FATFS_INIT -> FATFS_LinkDriver_EX, extern SD_Driver가 disk로 호출됨, disk_write --->>> SD_write)
2. main | 센서 내부 계산 동작 -> 기록할 내용을 RAM에 저장
3. 원하는 상황에 (ex. 버퍼가 다 찰 경우, up to user) main.c -> f_write
4. f_write 내부 조건 만족시 DMA 까지 호출 
 - `ff.c:         f_write()`
 - `ff_gen_drv:   disk_write()`
 - `sd_diskio.c:  SD_write()`
 - `bsp_driver:  BSP_SD_WriteBlocks_DMA()`
 - `HAL:         HAL_SD_WriteBlocks_DMA()`
 - `HW:          SDIO + DMA 전송`

  

## Hardware Configuration

- **Board**: STM32 NUCLEO-F411RE
- **SD Interface**: SDIO (4-bit wide)
- **Clock**:
  - SDIO clock divider configured for safe initialization (PLLCLK = SDIOCLK = 48Mhz)
    init 때는 400khz sdio clk 맞추기 위해 **ClockDiv = 118로 하는 레퍼런스 존재, but 필수 아님**  
  - Increased after card initialization for data transfer  
     SDIO_CK = SDIOCLK / (ClockDiv + 2), ClockDiv = 0 --> 2  
     main 중간에 변경을 위한 함수 호출함.
    <img width="1564" height="510" alt="image" src="https://github.com/user-attachments/assets/f688a7f1-4236-4b88-829c-35359ab21861" />

- **DMA**:
  - SDIO RX/TX connected to DMA controller

- **NVIC**:
  - SDIO global interrupt enabled
  - DMA stream interrupts enabled
- **Pin Mapping**:
  - CMD, CLK, DAT[0:3] mapped according to STM32F411 SDIO pinout
  - External pull-ups required on CMD and DAT lines (SD card side)
 
    <img width="706" height="630" alt="image" src="https://github.com/user-attachments/assets/b66d57c6-3538-493e-9902-cd5851be9696" />

### Modified Codes
- **hsd 는 SD_HandleTypeDef hsd; **
- sdio.c 에서 선언됨, 나머지 파일들은 헤더에서 extern 으로 선언하여 사용

---------
**sdio.c -->> MX_SDIO_SD_INIT**: 
  `hsd.Init.BusWide = SDIO_BUS_WIDE_4B 로 시작`
  - 직후에 SDIO_BUS_WIDE_1B로 초기화   
  - main에서 INIT 단계 끝나고 ** fs mount ** 이후에 다시 SDIO_BUS_WIDE_4B로 전환
----------
**sd_diskio.c -->> WriteStatus 는 polling 대체**
  - `callback (interrupt) 발생 -> WriteStatus 변화`

**sd_diskio.c -->> #define DISABLE_SD_INIT 주석 해제**
 -  HAL 관련 코드, BSP 관련 코드, fatfs 관련 코드에서 SD inititalize가 중복으로 일어날 수 있음
 -  ` SD 카드 초기화는 시스템 INIT(MX_SDIO_SD_Init / BSP_SD_Init)에서 한 번만 수행,`
 -  `확실한 역할 분리를 위함`
---------
 -  이후 **STA_NOINIT**는 FatFs 레벨에서 디스크 접근을 허용하는 논리플래그
 -  상태판단용으로 사용 중  

- 실제 데이터 전송의 완료 및 에러 판단은 STA_NOINIT가 아니라 DMA 완료 콜백
- `(BSP_SD_WriteCpltCallback, BSP_SD_ReadCpltCallback)과 에러 콜백(BSP_SD_ErrorCallback)에서 갱신되는 WriteStatus / ReadStatus 값`

--------
## 발생했던 문제들
#### 1. sdio.c <-> stm32f4xx_hal_sd.c
<img width="635" height="254" alt="image" src="https://github.com/user-attachments/assets/2c397bef-2ece-49db-bf05-e68fb92c6dad" />

<img width="679" height="316" alt="image" src="https://github.com/user-attachments/assets/4138197c-23a4-4906-9094-1ab07df4e2f1" />   

`위는 sdio.c의 MX_SDIO_SD_Init(void),  아래는 stm32f4xx_hal_sd.c의 HAL_SD_InitCard(SD_HandleTypeDef *hsd)`   

`이 둘이 불일치할 경우 main 에서 mount 실패한다.`  
`(+ clkdiv (클럭 속도 조절)의 경우 init 이후 main 도중에 바꿔도 작동 문제 없었다)`
`많은 레퍼런스에서는 init clkdiv = 0x76 (=118), 전송 중에는 clkdiv를 0에 가깝게 설정한다`



#### 2. Initialize 순서
- HAL - SystemClock_Config - GPIO - (UART)  (기본 세팅)
- DMA -> SDIO_SD -> FATFS


  
#### 3. 배선 - PCB 설계 시 참조
- 다른 레퍼런스에서 언급하는 문제: CLK 이외 전부 pull-up 저항 직렬연결 필요
- CLK 선은 짧아야 하며, GND 근처에 배치하기
- CLK 선이 불가피하게 길 경우 저속 클럭을 사용

- 
4.  


## License

This project is released under the MIT License.

It includes STM32 HAL and BSP drivers provided by STMicroelectronics,
which are licensed under ST’s BSD-style license.

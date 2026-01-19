/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : SDIO + FatFs (SAFE FLOW)
  *                  1bit init -> mount -> (optional) switch to 4bit
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "gpio.h"
#include "usart.h"
#include "sdio.h"
#include "fatfs.h"

/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "bsp_driver_sd.h"
#include "ff.h"
/* USER CODE END Includes */

/* USER CODE BEGIN PV */
extern SD_HandleTypeDef hsd;/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "gpio.h"
#include "usart.h"
#include "sdio.h"
#include "fatfs.h"
#include "dma.h"   // ✅ 안전벨트: DMA 템플릿/IRQ 환경에서 문제 줄임

/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "ff.h"
/* USER CODE END Includes */

/* USER CODE BEGIN PV */
extern SD_HandleTypeDef hsd;

extern FATFS SDFatFS;
extern FIL   SDFile;
extern char  SDPath[4];   // "0:/"

static uint32_t g_seq = 0;
/* USER CODE END PV */

void SystemClock_Config(void);

/* USER CODE BEGIN 0 */
static void die(const char *tag, int code)
{
  printf("[DIE] %s code=%d hsd_err=0x%08lX\r\n",
         tag, code, (unsigned long)HAL_SD_GetError(&hsd));
  Error_Handler();
}
/* USER CODE END 0 */

int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_DMA_Init();        // ✅ BP0: DMA2 clock + NVIC (있어도 손해 없음)
  MX_SDIO_SD_Init();    // SDIO 핀/인스턴스 세팅(핵심 init은 아래 HAL_SD_Init)
  MX_FATFS_Init();      // FatFs driver link

  printf("\r\n[BOOT] SDIO + FatFs minimal (1bit init -> mount -> optional 4bit)\r\n");

  /* ========================= */
  /* [BP1] SD CARD INIT (ONCE) */
  /* ========================= */
  // ✅ BP1: 여기 걸고 hsd.Init.BusWide / ClockDiv 확인
  if (HAL_SD_Init(&hsd) != HAL_OK)
    die("HAL_SD_Init", 1);

  // 1bit 강제(원하면 명시적으로)
  // (대부분 MX_SDIO_SD_Init에서 1bit로 시작하게 되어있음)
  // HAL_SD_ConfigWideBusOperation(&hsd, SDIO_BUS_WIDE_1B);

  /* ===================== */
  /* [BP2] FATFS MOUNT      */
  /* ===================== */
  // ✅ BP2: 여기서 fr이 FR_DISK_ERR면 "diskio read 실패" 쪽(거의 sd_diskio.c/IRQ/클럭)
  FRESULT fr = f_mount(&SDFatFS, (TCHAR const*)SDPath, 1);
  if (fr != FR_OK)
  {
    printf("[FATFS] mount fail fr=%d hsd_err=0x%08lX\r\n",
           (int)fr, (unsigned long)HAL_SD_GetError(&hsd));
    die("f_mount", (int)fr);
  }
  printf("[FATFS] mount OK\r\n");

  /* ============================= */
  /* [BP3] OPTIONAL: SWITCH 4-BIT  */
  /* ============================= */
  // ✅ BP3: 4bit 실패하면 배선/D1~D3 풀업/신호무결성 쪽 가능성 큼 → 그냥 1bit로 계속 가
  hsd.Init.ClockDiv = 2;   // 48MHz / 6 = 8MHz
hsd.Init.BusWide = SDIO_BUS_WIDE_4B;

//  if (HAL_SD_ConfigWideBusOperation(&hsd, SDIO_BUS_WIDE_4B) == HAL_OK)
//    printf("[SDIO] 4bit OK\r\n");
//  else
//    printf("[SDIO] 4bit FAIL -> keep 1bit (hsd_err=0x%08lX)\r\n",
//           (unsigned long)HAL_SD_GetError(&hsd));

  /* ===================== */
  /* [BP4] FILE OPEN/APPEND */
  /* ===================== */
  // ✅ BP4: 여기서부터는 파일시스템 레벨
  fr = f_open(&SDFile, "log.txt", FA_OPEN_ALWAYS | FA_WRITE);
  if (fr != FR_OK) die("f_open", (int)fr);

  fr = f_lseek(&SDFile, f_size(&SDFile));
  if (fr != FR_OK) die("f_lseek", (int)fr);

  if (f_size(&SDFile) == 0U)
  {
    UINT bw;
    const char *hdr = "t_ms,seq,hsd_err\r\n";
    fr = f_write(&SDFile, hdr, (UINT)strlen(hdr), &bw);
    if (fr != FR_OK || bw != (UINT)strlen(hdr)) die("header_write", (int)fr);
    (void)f_sync(&SDFile);
  }

  /* ============ LOOP ============ */
  while (1)
  {
    char line[96];
    UINT bw;
   volatile UINT dummy = 0;

    uint32_t t   = HAL_GetTick();
    uint32_t err = HAL_SD_GetError(&hsd);

    int n = snprintf(line, sizeof(line),
                     "%lu,%lu,0x%08lX\r\n",
                     (unsigned long)t,
                     (unsigned long)g_seq++,
                     (unsigned long)err);

    fr = f_write(&SDFile, line, (UINT)n, &bw);
    if (fr != FR_OK || bw != (UINT)n) die("f_write", (int)fr);

    if ((g_seq % 5000U) == 0U)
    {
      fr = f_sync(&SDFile);
      if (fr != FR_OK) die("f_sync", (int)fr);
    }

    dummy++;
  }
}

/**
  * @brief System Clock Configuration
  * @note  안전/단순 우선: 72MHz (HSE BYPASS + PLL)
  *       - F411에서 무난
  *       - SDIO는 보통 48MHz 도메인(PLLQ/PLLI2S) 영향이 있으니,
  *         SDIO 클럭이 이상하면 여기부터 점검.
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState       = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;

  // HSE=8MHz 가정
  // VCO = 8/4*72 = 144MHz
  // SYSCLK = 144/2 = 72MHz
  // PLL48CLK = 144/3 = 48MHz  ✅ (SDIO/USB 쪽)
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 72;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 3;

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    Error_Handler();

  RCC_ClkInitStruct.ClockType =
      RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
      RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2; // ✅ APB1 36MHz
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1; // ✅ APB2 72MHz

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    Error_Handler();
}

void Error_Handler(void)
{
  __disable_irq();
  while (1) { }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  (void)file;
  (void)line;
}
#endif

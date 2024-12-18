/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2023 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "iwdg.h"
#include "tim.h"
#include "usart.h"
#ifdef DVM_V24_V1
#include "usb_device.h"
#endif
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "leds.h"
#include "log.h"
#include "log.h"
#include "serial.h"
#include "sync.h"
#include "util.h"
#include "vcp.h"
#include "hdlc.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
unsigned long rxValidFrames = 0;
unsigned long rxTotalFrames = 0;
unsigned long txTotalFrames = 0;

// LED timers
unsigned long lastHb = 0;
unsigned long lastSync = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

#ifdef DVM_V24_V1 // HB LED is only on DVM-V24 V1 boards
void hbLED()
{
    if (HAL_GetTick() - lastHb > 1000)
    {
        LED_HB(1);
        lastHb = HAL_GetTick();
    }
    else if (HAL_GetTick() - lastHb > 100)
    {
        LED_HB(0);
    }
}
#endif

void linkLED()
{
    if (SyncRxState == SYNCED)
    {
        LED_LINK(1);
    }
    else
    {
        LED_LINK(0);
    }
}

#ifndef DVM_V24_V1

/**
 * @brief Jump to the STM32 USART1 system bootloader
 *
 * Taken from the dvmfirmware-hs function of the same name
 */
void jumpToBootloader()
{
    // De-init RCC
    HAL_RCC_DeInit();

    // De-init TIM2
    //HAL_TIM_Base_DeInit(&htim2);

    // De-init USART1 & 2 (also de-inits DMA)
    HAL_UART_MspDeInit(&huart1);
    HAL_UART_MspDeInit(&huart2);

    // Disable Systick timer
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;

    // Clear Interrupt Enable Register & Interrupt Pending Register
    for (uint8_t i = 0; i < sizeof(NVIC->ICER) / sizeof(NVIC->ICER[0]); i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }

    volatile uint32_t addr = 0x1FFFF000;

    // Update the NVIC's vector
    SCB->VTOR = addr;

    void (*SysMemBootJump)(void);
    SysMemBootJump = (void (*)(void))(*((uint32_t *)(addr + 4)));
    __ASM volatile("MSR msp, %0" : : "r"(*(uint32_t *)addr) : "sp"); // __set_MSP
    SysMemBootJump();
}

#endif

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{

    /* USER CODE BEGIN 1 */
#ifndef DVM_V24_V1
    // Check if we need to jump straight to the system bootloader (borrowed from dvmfirmware-hs main())
    if ((STM32_CNF_PAGE[STM32_CNF_PAGE_24] != 0xFFFFFFFFU) && (STM32_CNF_PAGE[STM32_CNF_PAGE_24] != 0x00U))
    {
        // Check if the bootload mode flag is set
        uint8_t bootloadMode = (STM32_CNF_PAGE[STM32_CNF_PAGE_24] >> 8) & 0xFFU;
        if ((bootloadMode & 0x20U) == 0x20U)
        {
            // Setup the erase struct
            static FLASH_EraseInitTypeDef EraseInitStruct;
            // Storage for errors from the erase command
            uint32_t sectorError;
            // Unlock Flash
            HAL_FLASH_Unlock();
            // Clear the config page
            EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
            EraseInitStruct.PageAddress = STM32_CNF_PAGE_ADDR;
            EraseInitStruct.NbPages = 1;
            if (HAL_FLASHEx_Erase(&EraseInitStruct, &sectorError) != HAL_OK)
            {
                HAL_FLASH_Lock();
                return HAL_FLASH_GetError();
            }
            // Jump
            jumpToBootloader();
        }
    }
#endif
    /* USER CODE END 1 */

    /* MCU Configuration--------------------------------------------------------*/

    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

    /* USER CODE BEGIN Init */

    /* USER CODE END Init */

    /* Configure the system clock */
    SystemClock_Config();

    /* USER CODE BEGIN SysInit */

    /* USER CODE END SysInit */

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART2_UART_Init();
    MX_TIM2_Init();
#ifdef DVM_V24_V1
    MX_USB_DEVICE_Init();
#else
    MX_USART1_UART_Init();
#endif
    MX_IWDG_Init();
    /* USER CODE BEGIN 2 */
    // Init stuff
    log_set_uart(&huart2);
    // SerialClear(&huart2);
    SerialStartup(&huart2);

// Enumerate the VCP
// log_info("Enumerating USB VCP");
#ifdef DVM_V24_V1
    VCPEnumerate();
#endif

    // Start sync serial handler
    log_info("Starting synchronous serial handler");
    SyncStartup(&htim2);

    // Done!
    log_info("Startup complete");
    VCPWriteDebug1("Startup complete");
    SyncReset();

// Warn that watchdog is disbaled
#ifdef DISABLE_WATCHDOG
    log_warn("System watchdog timers disbaled!");
#endif

    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1)
    {
        // Processing callbacks
        RxMessageCallback();
        HdlcCallback();
        VCPRxCallback();
        VCPTxCallback();
        SerialCallback(&huart2);
// LED callbacks
#ifdef DVM_V24_V1
        hbLED();
#endif
        linkLED();
// Refresh the IWDG watchdog
#ifndef DISABLE_WATCHDOG
        HAL_IWDG_Refresh(&hiwdg);
#endif
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
    }
    /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI | RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.LSIState = RCC_LSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
    PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
        Error_Handler();
    }
}

/* USER CODE BEGIN 4 */

// Override for calling sync callback
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    SyncTimerCallback();
}

/**
 * @brief Reset the STM32
 *
 */
void ResetMCU()
{
    //log_warn("MCU is resetting!");
    //HAL_Delay(250);
    LED_ACT(0);
    LED_LINK(0);
#ifdef DVM_V24_V1
    LED_HB(0);
    LED_USB(0);
#else
    LED_USB_RX(0);
    LED_USB_TX(0);
#endif
    HAL_Delay(250);
    HAL_NVIC_SystemReset();
}

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    while (1)
    {
    }
    /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
       ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/**
  ******************************************************************************
  * @file           : config.h
  * @brief          : Global config variables/defines
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __CONFIG_H
#define __CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

// Global logging level switches
#define SHOW_DEBUG
#define SHOW_TRACE

// Synchronous Serial Logging
//#define DEBUG_SYNC
//#define TRACE_SYNC

// HDLC framing logging
#define INFO_HDLC
//#define DEBUG_HDLC
//#define TRACE_HDLC

// Virtual Com Port (VCP) logging
//#define DEBUG_VCP_RX
//#define DEBUG_VCP_TX
//#define TRACE_VCP

// Enable periodic status print
//#define PERIODIC_STATUS
// Interval in ms for the periodic status print
#define PERIODIC_STATUS_INT 30000

// Report buffer space in 16-byte blocks instead of LDUs
#define STATUS_SPACE_BLOCKS

// STM32 Interrupt Priorities
#define NVIC_PRI_TIM2           2U
#define NVIC_PRI_USART1_TX      3U
#define NVIC_PRI_USART1_RX      4U
#define NVIC_PRI_USART2_DMA     5U
#define NVIC_PRI_USART2_INT     6U

// Flash Areas (shamelessly stolen from dvmfirmware-hs)
#define STM32_CNF_PAGE_ADDR     (uint32_t)0x0800FC00
#define STM32_CNF_PAGE          ((uint32_t *)0x0800FC00)
#define STM32_CNF_PAGE_24       24U

// Time in ms above which critical routines will throw a warning
#define FUNC_TIMER_WARN     10U

#define FW_MAJ  "2"
#define FW_MIN  "3"
#define FW_REV  "0"

#ifdef DVM_V24_V1
#define VERSION_STRING      "DVM-V24-V1 FW V" FW_MAJ "." FW_MIN "." FW_REV " (" GIT_HASH ")"
#else
#define VERSION_STRING      "DVM-V24-V2 FW V" FW_MAJ "." FW_MIN "." FW_REV " (" GIT_HASH ")"
#endif
#define BUILD_DATE_STRING   __DATE__ " " __TIME__
#define HARDWARE_STRING     VERSION_STRING ", " BUILD_DATE_STRING

// P25 Frame sizes
#define P25_LDU_FRAME_LENGTH_BYTES      216U
#define P25_V24_LDU_FRAME_LENGTH_BYTES  370U
#define VCP_MAX_MSG_LENGTH_BYTES        255U

#ifdef __cplusplus
}
#endif

#endif
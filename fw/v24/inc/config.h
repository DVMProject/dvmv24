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

// Sanity check
#if !defined DVM_V24_V1 && !defined DVM_V24_V2
#error "Invalid compile-time environment! Missing one of DVM_V24_V1 or DVM_V24_V2"
#endif

// Report buffer space in 16-byte blocks instead of LDUs
#define STATUS_SPACE_BLOCKS

// STM32 Interrupt Priorities
#define NVIC_PRI_TIM2       3U
#define NVIC_PRI_USB_USART1 4U
#define NVIC_PRI_DMA        5U
#define NVIC_PRI_USART2     6U

// Time in ms above which critical routines will throw a warning
#define FUNC_TIMER_WARN     10U

#define FW_MAJ  "2"
#define FW_MIN  "2"

#ifdef DVM_V24_V2
#define VERSION_STRING      "DVM-V24-V2 FW V" FW_MAJ "." FW_MIN " (" GIT_HASH ")"
#else
#define VERSION_STRING      "DVM-V24 FW V" FW_MAJ "." FW_MIN " (" GIT_HASH ")"
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
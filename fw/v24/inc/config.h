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

// Logging sections
//#define DEBUG_SYNC
//#define TRACE_SYNC

#define INFO_HDLC
//#define DEBUG_HDLC
//#define TRACE_HDLC

//#define DEBUG_VCP
//#define TRACE_VCP

#define VERSION_STRING  "1.0"
#define DATE_STRING     __DATE__ " " __TIME__

#define PERIODIC_STATUS

#ifdef __cplusplus
}
#endif

#endif
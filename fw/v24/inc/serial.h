/**
  ******************************************************************************
  * @file           : serial.h
  * @brief          : Header for serial.c file
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __SERIAL_H
#define __SERIAL_H

#ifdef __cplusplus
extern "C" {
#endif

#define SERIAL_BUFFER_SIZE  1024

// Self-serving fancy startup text
#define W3AXL_LINE1 "\x1b[36m   _       _______ ___   _  __ __ "
#define W3AXL_LINE2         "  | |     / /__  //   | | |/ // / "
#define W3AXL_LINE3         "  | | /| / / /_ </ /| | |   // /  "
#define W3AXL_LINE4         "  | |/ |/ /___/ / ___ |/   |/ /___"
#define W3AXL_LINE5         "  |__/|__//____/_/  |_/_/|_/_____/\x1b[0m"

// needed for standard HAL operations
#include "stm32f1xx_hal.h"

// standard c libs
#include <stdint.h>
#include <string.h>
#include "main.h"

// External functions
void SerialCallback(UART_HandleTypeDef *huart);
void SerialWrite(UART_HandleTypeDef *huart, const char *data);
void SerialWriteLn(UART_HandleTypeDef *huart, const char *data);
void SerialClear(UART_HandleTypeDef *huart);
void SerialStartup(UART_HandleTypeDef *huart);

#ifdef __cplusplus
}
#endif

#endif

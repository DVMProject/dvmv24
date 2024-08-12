/**
  ******************************************************************************
  * @file           : fault.h
  * @brief          : Fault handling utilities for STM32
  * 
  * This was created from the guide at
  * https://rhye.org/post/stm32-with-opencm3-5-fault-handlers/
  ******************************************************************************
*/

#ifndef __FAULT_H
#define __FAULT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <string.h>

void base_fault_handler(uint32_t stack[]);

#ifdef __cplusplus
}
#endif

#endif
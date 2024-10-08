/**
  ******************************************************************************
  * @file           : util.h
  * @brief          : Header for util.c file
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __UTIL_H
#define __UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <string.h>
#include "log.h"

// this converts to string
#define STR_(X) #X

// this makes sure the argument is expanded before converting to string
#define STR(X) STR_(X)

#define STM32_UUID ((uint32_t *)0x1FFFF7E8)

typedef struct {
    uint8_t const buffer;
    int head;
    int tail;
    const int maxlen;
} CircularBuffer_t;

void HexArrayToStr(char *buf, uint8_t *array, uint8_t len);
bool GetBitAtPos(uint8_t byte, uint8_t pos);
uint8_t SetBitAtPos(uint8_t byte, uint8_t pos, bool value);
void printHexArray(char *outBuf, uint8_t *array, uint8_t len);
unsigned char reverseBits(unsigned char b);
void getUid(uint8_t* buffer);
void getUidString(char *str);
void getCPU();

#ifdef __cplusplus
}
#endif

#endif
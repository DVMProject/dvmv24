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

#ifdef __cplusplus
}
#endif

#endif
/**
  ******************************************************************************
  * @file           : fifo.h
  * @brief          : Header for fifo.c file
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __FIFO_H
#define __FIFO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "log.h"

typedef struct {
    uint8_t * const buffer;
    int size;
    int head;
    int tail;
    const int maxlen;
} FIFO_t;

int FifoPush(FIFO_t *c, uint8_t data);
int FifoPop(FIFO_t *c, uint8_t *data);
int FifoPeek(FIFO_t *c, uint8_t *data);
void FifoClear(FIFO_t *c);

#ifdef __cplusplus
}
#endif

#endif
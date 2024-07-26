/**
  ******************************************************************************
  * @file           : sync.h
  * @brief          : Header for sync.c file
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __SYNC_H
#define __SYNC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"
#include "stdint.h"
#include "log.h"
#include "main.h"

#define SYNC_TX_DELAY   32      // ms to wait once TX fifo has data before we start to send
#define SYNC_RX_DELAY   1000    // ms to wait after startup/reset before starting RX sync routines

// HDLC parameters
#define HDLC_BIT_STUFFED    0x7C    // (b01111100) we skip the next bit in this case (we've received 5 1s in a row)
#define HDLC_SYNC_WORD      0x7E    // (b01111110) we search for this while we bitshift our rx byte buffer
#define HDLC_INVALID        0xFE    // (b01111111) this should never happen and we should throw ourselves out of sync if we see it
#define HDLC_ESCAPE_CODE    0x7D    // This is used to escape bytes
#define HDLC_ESCAPE_7E      0x5E    // Follows the escape code to escape a 0x7E
#define HDLC_ESCAPE_7D      0x5D    // Follows the escape code to escape a 0x7D

#define SYNC_RX_BUF_LEN (P25_V24_LDU_FRAME_LENGTH_BYTES * 2)
#define SYNC_TX_BUF_LEN (P25_V24_LDU_FRAME_LENGTH_BYTES * 2)

// Pin Definitions (pin names are labelled in STM32CubeMX projet)
#define GET_RXCLK(state)    HAL_GPIO_ReadPin(DCE_RXCLK_GPIO_Port, DCE_RXCLK_Pin)
#define GET_RXD()           HAL_GPIO_ReadPin(DCE_RXD_GPIO_Port, DCE_RXD_Pin)
#define GET_CTS()           HAL_GPIO_ReadPin(DCE_CTS_GPIO_Port, DCE_CTS_Pin)
#define SET_TXCLK(state)    HAL_GPIO_WritePin(DCE_TXCLK_GPIO_Port, DCE_TXCLK_Pin, state)
#define SET_TXD(state)      HAL_GPIO_WritePin(DCE_TXD_GPIO_Port, DCE_TXD_Pin, state)
#define SET_CTS(state)      HAL_GPIO_WritePin(DCE_CTS_GPIO_Port, DCE_CTS_Pin, state)

#define TXCLK_HIGH()    SET_TXCLK(1)
#define TXCLK_LOW()     SET_TXCLK(0)

// State machine stuff
enum RxState {
    SEARCH,
    SYNCED
};

void SyncStartup(TIM_HandleTypeDef *tim);
void SyncResetRx();
void SyncDrop();
void SyncTimerCallback(void);
bool SyncAddTxByte(const uint8_t byte);
bool SyncAddTxBytes(const uint8_t *bytes, int len);
uint8_t SyncGetTxFree();
void RxMessageCallback();
uint16_t StuffByte(uint8_t byte);

#ifdef __cplusplus
}
#endif

#endif
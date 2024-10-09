/**
  ******************************************************************************
  * @file           : vcp.h
  * @brief          : Header for vcp.c file
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __VCP_H
#define __VCP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"
#include "stdint.h"
#include "main.h"
#include "log.h"
#include "sync.h"

#define VCP_RX_BUF_LEN      (P25_V24_LDU_FRAME_LENGTH_BYTES * 4)
#define VCP_TX_BUF_LEN      (P25_V24_LDU_FRAME_LENGTH_BYTES * 2)

#define VCP_RX_TIMEOUT      100

#define USB_ENUM(state)    HAL_GPIO_WritePin(USB_ENUM_GPIO_Port, USB_ENUM_Pin, state)

#define USB_TX_RETRIES      3

// DVM Serial Protocol Defines
enum DVM_COMMANDS {
    CMD_GET_VERSION         = 0x00,
    CMD_GET_STATUS          = 0x01,
    CMD_SET_MODE            = 0x03,
    CMD_CAL_DATA            = 0x08,
    CMD_P25_DATA            = 0x31,
    CMD_P25_LOST            = 0x32,
    CMD_P25_CLEAR           = 0x33,
    CMD_ACK                 = 0x70,
    CMD_NAK                 = 0x7F,
    CMD_DEBUG1              = 0xF1,
    CMD_DEBUG2              = 0xF2,
    CMD_DEBUG3              = 0xF3,
    CMD_DEBUG4              = 0xF4,
    CMD_DEBUG5              = 0xF5,
    CMD_DEBUG_DUMP          = 0xFA,
    DVM_LONG_FRAME_START    = 0xFD,
    DVM_SHORT_FRAME_START   = 0xFE,
};

// DVM status states
enum DVM_STATE {
    STATE_IDLE = 0U,                    //! Idle
    STATE_P25 = 2U,                     //! Project 25
};

// Vars for V2 serial implementation
#ifndef DVM_V24_V1
bool usartRx = false;
uint8_t usartRxBuffer[8] = {0};
#endif

void VCPRxITCallback(uint8_t* buf, uint32_t len);
void VCPRxCallback();

void VCPTxCallback();

bool VCPWrite(uint8_t *data, uint16_t len);
bool VCPWriteAck(uint8_t cmd);
bool VCPWriteP25Frame(const uint8_t *data, uint16_t len);

void sendVersion();
void sendStatus();

bool VCPWriteDebug1(const char *text);
bool VCPWriteDebug2(const char *text, int16_t n1);
bool VCPWriteDebug3(const char *text, int16_t n1, int16_t n2);
bool VCPWriteDebug4(const char *text, int16_t n1, int16_t n2, int16_t n3);

void VCPEnumerate();

#ifdef __cplusplus
}
#endif

#endif
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

#define VCP_TX_BUF_LEN      1024
#define VCP_RX_BUF_LEN      1024

#define USB_ENUM(state)    HAL_GPIO_WritePin(USB_ENUM_GPIO_Port, USB_ENUM_Pin, state)

// DVM Serial Protocol Defines
enum DVM_COMMANDS {
    CMD_GET_VERSION = 0x00,
    CMD_GET_STATUS  = 0x01,
    CMD_P25_DATA    = 0x31,
    CMD_P25_LOST    = 0x32,
    CMD_P25_CLEAR   = 0x33,
    CMD_ACK         = 0x70,
    CMD_NAK         = 0x7F,
    CMD_DEBUG1      = 0xF1,
    CMD_DEBUG2      = 0xF2,
    CMD_DEBUG3      = 0xF3,
    CMD_DEBUG4      = 0xF4,
    CMD_DEBUG5      = 0xF5,
    CMD_DEBUG_DUMP  = 0xFA,
    DVM_FRAME_START = 0xFE,
};

void VCPRxITCallback(uint8_t* buf, uint32_t len);
void VCPCallback();
bool VCPWrite(uint8_t *data, uint16_t len);
bool VCPWriteP25Frame(const uint8_t *data, uint16_t len);
void VCPEnumerate();

#ifdef __cplusplus
}
#endif

#endif
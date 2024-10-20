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
#define VCP_TX_TIMEOUT      100

#define USB_ENUM(state)     HAL_GPIO_WritePin(USB_ENUM_GPIO_Port, USB_ENUM_Pin, state)

#define USB_TX_RETRIES      3

// DVM Serial Protocol Defines
enum DVM_COMMANDS {
    CMD_GET_VERSION         = 0x00,
    CMD_GET_STATUS          = 0x01,
    CMD_SET_CONFIG          = 0x02,
    CMD_SET_MODE            = 0x03,
    CMD_SET_RFPARAMS        = 0x06,
    CMD_CAL_DATA            = 0x08,
    CMD_P25_DATA            = 0x31,
    CMD_P25_LOST            = 0x32,
    CMD_P25_CLEAR           = 0x33,
    CMD_ACK                 = 0x70,
    CMD_NAK                 = 0x7F,
    CMD_FLASH_READ          = 0xE0,
    CMD_FLASH_WRITE         = 0xE1,
    CMD_RESET_MCU           = 0xEA,
    CMD_DEBUG1              = 0xF1,
    CMD_DEBUG2              = 0xF2,
    CMD_DEBUG3              = 0xF3,
    CMD_DEBUG4              = 0xF4,
    CMD_DEBUG5              = 0xF5,
    CMD_DEBUG_DUMP          = 0xFA,
    DVM_LONG_FRAME_START    = 0xFD,
    DVM_SHORT_FRAME_START   = 0xFE,
};

// DVM response reason codes
enum DVM_REASONS {
    RSN_OK = 0U,                        //! OK
    RSN_NAK = 1U,                       //! Negative Acknowledge

    RSN_ILLEGAL_LENGTH = 2U,            //! Illegal Length
    RSN_INVALID_REQUEST = 4U,           //! Invalid Request
    RSN_RINGBUFF_FULL = 8U,             //! Ring Buffer Full

    RSN_INVALID_FDMA_PREAMBLE = 10U,    //! Invalid FDMA Preamble Length
    RSN_INVALID_MODE = 11U,             //! Invalid Mode

    RSN_INVALID_DMR_CC = 12U,           //! Invalid DMR CC
    RSN_INVALID_DMR_SLOT = 13U,         //! Invalid DMR Slot
    RSN_INVALID_DMR_START = 14U,        //! Invaild DMR Start Transmit
    RSN_INVALID_DMR_RX_DELAY = 15U,     //! Invalid DMR Rx Delay

    RSN_INVALID_P25_CORR_COUNT = 16U,   //! Invalid P25 Correlation Count

    RSN_NO_INTERNAL_FLASH = 20U,        //! No Internal Flash
    RSN_FAILED_ERASE_FLASH = 21U,       //! Failed to erase flash partition
    RSN_FAILED_WRITE_FLASH = 22U,       //! Failed to write flash partition
    RSN_FLASH_WRITE_TOO_BIG = 23U,      //! Data to large for flash partition

    RSN_HS_NO_DUAL_MODE = 32U,          //! (Hotspot) No Dual Mode Operation

    RSN_DMR_DISABLED = 63U,             //! DMR Disabled
    RSN_P25_DISABLED = 64U,             //! Project 25 Disabled
    RSN_NXDN_DISABLED = 65U             //! NXDN Disabled
};

// DVM status states
enum DVM_STATE {
    STATE_IDLE = 0U,                    //! Idle
    STATE_P25 = 2U,                     //! Project 25
};

#ifdef DVM_V24_V1
void VCPEnumerate();
void VCPRxITCallback(uint8_t* buf, uint32_t len);
#else
void VCPTxComplete();
#endif

void VCPRxCallback();
void VCPTxCallback();

bool VCPWrite(uint8_t *data, uint16_t len);
bool VCPWriteAck(uint8_t cmd);
bool VCPWriteNak(uint8_t cmd, uint8_t err);
bool VCPWriteP25Frame(const uint8_t *data, uint16_t len);

void sendVersion();
void sendStatus();
#ifndef DVM_V24_V1
void flashRead();
uint8_t flashWrite(const uint8_t* data, uint8_t length);
#endif

bool VCPWriteDebug1(const char *text);
bool VCPWriteDebug2(const char *text, int16_t n1);
bool VCPWriteDebug3(const char *text, int16_t n1, int16_t n2);
bool VCPWriteDebug4(const char *text, int16_t n1, int16_t n2, int16_t n3);



#ifdef __cplusplus
}
#endif

#endif
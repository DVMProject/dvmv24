/**
  ******************************************************************************
  * @file           : vcp.c
  * @brief          : Asynchronous VCP serial handling
  ******************************************************************************
  */

// self-referential include
#include "vcp.h"

#include "leds.h"
#include "stdio.h"
#include "usbd_cdc_if.h"
#include "log.h"
#include "fifo.h"
#include "util.h"
#include "config.h"
#include "string.h"
#include "hdlc.h"

uint8_t vcpRxMsg[VCP_RX_BUF_LEN];

extern bool USB_VCP_DTR;

// VCP TX Fifo
uint8_t vcpTxBuf[VCP_TX_BUF_LEN];
FIFO_t vcpTxFifo = {
    .buffer = vcpTxBuf,
    .head = 0,
    .tail = 0,
    .maxlen = VCP_TX_BUF_LEN
};

// VCP RX Fifo
uint8_t vcpRxBuf[VCP_RX_BUF_LEN];
FIFO_t vcpRxFifo = {
    .buffer = vcpRxBuf,
    .head = 0,
    .tail = 0,
    .maxlen = VCP_RX_BUF_LEN
};

/**
 * @brief Called by the CDC_Receive_FS interrupt callback and fills the FIFO with the received bytes
*/
void VCPRxITCallback(uint8_t* buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
    {
        if (FifoPush(&vcpRxFifo, buf[i]))
        {
            log_error("VCP RX FIFO full!");
        }
    }
}

/**
 * @brief Called during main loop to handle any data received from USB
*/
void VCPCallback()
{
    // RX Buffer First
    // Wait for the start frame byte
    uint8_t byte = 0;
    // Try to read until either the buffer is empty or we get a start frame
    while (!FifoPop(&vcpRxFifo, &byte) && (byte != DVM_FRAME_START)) {}
    
    // if we got the start frame, read the rest of the message
    if (byte == DVM_FRAME_START)
    {
        LED_USB(1);
        // Get the length which should be the next byte
        uint8_t length;
        if (FifoPop(&vcpRxFifo, &length))
        {
            log_error("Reached end of VCP RX fifo before length byte was read");
            return;
        }
        // Get the command which should be next
        uint8_t command;
        if (FifoPop(&vcpRxFifo, &command))
        {
            log_error("Reached end of VCP RX fifo before command byte was read");
            return;
        }
        // Switch on the command
        if (command == CMD_P25_DATA)
        {
            // pop the 0x00 pad byte to nowhere
            uint8_t pad;
            if (FifoPop(&vcpRxFifo, &pad))
            {
                log_error("Reached end of VCP RX fifo before pad byte was read");
                return;
            }
            // read length - 4 bytes (accounting for start byte, length byte, command byte, and 0x00 pad byte)
            uint8_t p25data[length - 4];
            for (int i=0; i<length-4; i++)
            {
                // If we run out of fifo before the end of the message, return
                if (FifoPop(&vcpRxFifo, &p25data[i]))
                {
                    log_error("Reached end of VCP RX fifo before message was complete, expected %d and got %d", length - 4, i);
                    return;
                }
            }
            // Process the UI frame
            #ifdef DEBUG_VCP
            log_debug("Sending UI frame of length %d via HDLC", length - 4);
            #endif
            #ifdef TRACE_VCP
            uint8_t hexStrBuf[(length - 4)*4];
            HexArrayToStr((char*)hexStrBuf, &vcpRxMsg[4], length - 4);
            log_trace("P25 Frame: %s", hexStrBuf);
            #endif
            // Send the UI
            HDLCSendUI(p25data, length - 4);
        }
        LED_USB(0);
    }

    // TX next, similar but we don't care about the command
    // Buffer for TX message
    uint8_t txBuf[VCP_TX_BUF_LEN];
    // Wait for the start frame byte
    byte = 0;
    // Try to read until either the buffer is empty or we get a start frame
    while (!FifoPop(&vcpTxFifo, &byte) && (byte != DVM_FRAME_START)) {}
    
    // if we got the start frame, read the rest of the message
    if (byte == DVM_FRAME_START)
    {
        LED_USB(1);
        // Put start byte at index 0
        txBuf[0] = byte;
        // Get the length which should be the next byte
        uint8_t length;
        if (FifoPop(&vcpTxFifo, &length))
        {
            log_error("Reached end of VCP TX fifo before length byte was read");
            return;
        }
        // Put length byte in position 2
        txBuf[1] = length;
        // Write the rest of the bytes to the buffer
        for (uint16_t i = 2; i < length; i++)
        {
            if (FifoPop(&vcpTxFifo, &txBuf[i]))
            {
                log_error("Reached end of VCP TX fifo before entire message was read");
                return;
            }
        }
        // Send
        CDC_Transmit_FS(txBuf, length);
        LED_USB(0);
    }
}

/**
 * @brief write characters to the VCP
 * 
 * @param *data data to write
 * @param len length of data to write
 * 
 * @return true on success, false on failure
*/
bool VCPWrite(uint8_t *data, uint16_t len)
{
    for (int i = 0; i < len; i++)
    {
        if (FifoPush(&vcpTxFifo, data[i]))
        {
            return false;
        }
    }
    return true;
}

/**
 * @brief Write a P25 frame to the USB port, using the DVMHost format
 * 
 * @param *data data array to write
 * @param len length of data array
 * 
 * @return true on success, false on error (buffer full, etc)
*/
bool VCPWriteP25Frame(const uint8_t *data, uint16_t len)
{
    // Start byte, length byte, cmd byte, and a 0x00 pad to maintain dvm compliance
    uint8_t buffer[len + 4];
    buffer[0] = DVM_FRAME_START;
    buffer[1] = (uint8_t)len + 4;
    buffer[2] = CMD_P25_DATA;
    buffer[3] = 0x00;

    memcpy(buffer + 4, data, len);

    #ifdef DEBUG_VCP
    log_debug("Writing P25 frame of length %d to VCP", len);
    #endif
    #ifdef TRACE_VCP
    uint8_t hexStrBuf[(len+3)*4];
    printHexArray((char*)hexStrBuf, buffer, len+3);
    log_trace("Sending %s", hexStrBuf);
    #endif

    return VCPWrite(buffer, len+4);
}

/**
 * @brief Toggles the 1.5k resistor on D+ to renumerate the USB device
*/
void VCPEnumerate()
{
    USB_ENUM(0);
    HAL_Delay(50);
    USB_ENUM(1);
}
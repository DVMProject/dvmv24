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

const char HARDWARE[] = "DVM-V24 FW V" VERSION_STRING " (" DATE_STRING ")";

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
            log_error("VCP RX FIFO full! Clearing buffer");
            // Clear
            FifoClear(&vcpRxFifo);
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
    while (!FifoPop(&vcpRxFifo, &byte) && byte != DVM_SHORT_FRAME_START && byte != DVM_LONG_FRAME_START) {
        if (byte != 0) {
            log_warn("Got invalid byte from USB VCP: %02X", byte);
        }
    }
    
    // if we got the start frame, read the rest of the message
    if (byte == DVM_SHORT_FRAME_START || byte == DVM_LONG_FRAME_START)
    {
        LED_USB(1);

        uint8_t cmdOffset = 2;
        
        // Determine our length
        uint16_t length;
        // Short frame length is a single byte
        if (byte == DVM_SHORT_FRAME_START)
        {
            uint8_t len;
            // Pop the length byte
            if (FifoPop(&vcpRxFifo, &len))
            {
                log_error("Reached end of VCP RX fifo before length byte was read");
                return;
            }
            length = len;
        }
        // Long frame length is 2 bytes
        else
        {
            uint8_t len[2];
            if (FifoPop(&vcpRxFifo, &len[0]))
            {
                log_error("Reached end of VCP RX fifo before first length byte was read");
                return;
            }
            if (FifoPop(&vcpRxFifo, &len[1]))
            {
                log_error("Reached end of VCP RX fifo before second length byte was read");
                return;
            }
            // Calculate length
            length = ((len[0] & 0xFF) << 8) + (len[1] & 0xFF);
            // Update command byte offset
            cmdOffset = 3;
        }

        // Data length is total length minus start and length byte(s)
        uint16_t dataLength = length - cmdOffset;

        // Allocate space for the data
        uint8_t data[length];
        memset(data, 0x00, length);
        
        // Pre-populate
        if (byte == DVM_SHORT_FRAME_START)
        {
            data[0] = DVM_SHORT_FRAME_START;
            data[1] = length & 0xFF;
        } else {
            data[0] = DVM_LONG_FRAME_START;
            data[1] = (length >> 8) & 0xFF;
            data[2] = length & 0xFF;
        }
        
        // Get data
        for (int i = 0; i < dataLength; i++)
        {
            // If we run out of fifo before the end of the message, return
            if (FifoPop(&vcpRxFifo, &data[i + cmdOffset]))
            {
                log_error("Reached end of VCP RX fifo before message was complete, expected %d and got %d", dataLength, i);
                return;
            }
        }

        /*#ifdef TRACE_VCP
        log_debug("Received DVM message of length %d", length);
        uint8_t hexStrBuf[length*4];
        HexArrayToStr((char*)hexStrBuf, &data, length);
        log_trace("VCP RX Message: %s", hexStrBuf);
        #endif*/

        // Get the command which should be next
        uint8_t command = data[cmdOffset];
        
        // Switch on the command
        if (command == CMD_P25_DATA)
        {
            // Process the UI frame
            #ifdef DEBUG_VCP
            log_debug("Sending UI frame of length %d via HDLC", length - 4);
            #endif
            #ifdef TRACE_VCP
            uint8_t hexStrBuf[(length - 4)*4];
            HexArrayToStr((char*)hexStrBuf, &vcpRxMsg[4], length - 4);
            log_trace("P25 Frame: %s", hexStrBuf);
            #endif
            // Send the UI, ignoring the first 0x00 byte
            HDLCSendUI(data + cmdOffset + 2, length - cmdOffset - 2);
        }
        else if (command == CMD_GET_VERSION)
        {
            sendVersion();
        }
        else if (command == CMD_GET_STATUS)
        {
            sendStatus();
        }
        else if (command == CMD_SET_MODE)
        {
            #ifdef DEBUG_VCP
            log_debug("Ignoring CMD_SET_MODE, always in P25 mode");
            #endif
        }
        else if (command == CMD_P25_CLEAR)
        {
            // TODO: this
            #ifdef DEBUG_VCP
            log_debug("Ignoring CMD_P25_CLEAR, not yet implemented");
            #endif
        }
        else
        {
            log_warn("Unhandled DVM command %02X", command);
        }
        LED_USB(0);
    }

    // TX next, similar but we don't care about the command
    // Buffer for TX message
    uint8_t txBuf[VCP_TX_BUF_LEN];
    // Wait for the start frame byte
    byte = 0;
    // Try to read until either the buffer is empty or we get a start frame
    while (!FifoPop(&vcpTxFifo, &byte) && (byte != DVM_SHORT_FRAME_START)) {}
    
    // if we got the start frame, read the rest of the message
    if (byte == DVM_SHORT_FRAME_START)
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
            log_error("VCP TX FIFO full! Clearing.");
            FifoClear(&vcpTxFifo);
            return false;
        }
    }
    return true;
}

/**
 * @brief Write a P25 frame to the USB port, using the DVMHost modem format
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
    buffer[0] = DVM_SHORT_FRAME_START;
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
 * @brief Send the V24 board version and UID over the VCP
 * 
 * Stolen from the DVM modem firmware
*/
void sendVersion()
{
    uint8_t reply[200U];

    reply[0U] = DVM_SHORT_FRAME_START;
    reply[1U] = 0U;
    reply[2U] = CMD_GET_VERSION;
    reply[3U] = 0x04U;      // protocol version
    reply[4U] = 0x02U;      // CPU type (STM32)

    // 16-byte UDID
    memset(reply + 5U, 0x00U, 16U);
    getUid(reply + 5U);

    // HW description
    uint8_t count = 21;
    for (uint8_t i = 0U; i < sizeof(HARDWARE); i++, count++)
        reply[count] = HARDWARE[i];

    // Total length
    reply[1U] = count;

    VCPWrite(reply, count);

    #ifdef DEBUG_VCP
    log_info("Sent DVM version information");
    #endif
}

/**
 * @brief Send the current status of the V24 board
*/
void sendStatus()
{
    uint8_t reply[15U];
    memset(reply, 0x00U, 15U);

    // Frame start stuff
    reply[0U] = DVM_SHORT_FRAME_START;
    reply[1U] = 12U;
    reply[2U] = CMD_GET_STATUS;

    // Mode (always P25 mode)
    reply[3U] = 0x08U;

    // State (always p25)
    reply[4U] = 2U;

    // P25 tx buffer space
    reply[10U] = SyncGetTxFree();

    VCPWrite(reply, 15U);

    /*#ifdef TRACE_VCP
    log_info("Sent DVM status information");
    #endif*/
}

/**
 * @brief Write a debug message to the USB port, using the DVMHost modem format
 * 
 * @param *text string to write (must be properly formatted string with null terminator)
 * 
 * @return true on success, false on error
*/
bool VCPWriteDebug1(const char *text)
{
    // Get length of string
    uint8_t len = strlen(text);

    // Return on invalid string
    if (!len)
    {
        return false;
    }

    // Allocate the array 
    uint8_t *msg = malloc((len + 3) * sizeof *msg);

    // Make sure we alloc'd
    if (!msg)
    {
        return false;
    }

    // Populate header
    msg[0] = DVM_SHORT_FRAME_START;
    msg[1] = len + 3;
    msg[2] = CMD_DEBUG1;

    // Copy rest of message
    memcpy(msg + 3, text, len);

    // Send
    bool ret = VCPWrite(msg, len + 3);

    // Free
    free(msg);

    // Return
    return ret;
}

/**
 * @brief Write a debug message to the USB port with 1 trailing integer, using the DVMHost modem format
 * 
 * @param *text string to write (must be properly formatted string with null terminator)
 * @param n1 the number to write
 * 
 * @return true on success, false on error
*/
bool VCPWriteDebug2(const char *text, int16_t n1)
{
    // Get length of string
    uint8_t len = strlen(text);

    // Return on invalid string
    if (!len)
    {
        return false;
    }

    // Allocate the array (+3 for header, +2 for int)
    uint8_t *msg = malloc((len + 5) * sizeof *msg);

    // Make sure we alloc'd
    if (!msg)
    {
        return false;
    }

    // Populate header
    msg[0] = DVM_SHORT_FRAME_START;
    msg[1] = len + 5;
    msg[2] = CMD_DEBUG2;

    // Copy rest of message
    memcpy(msg + 3, text, len);

    // Copy the number
    msg[len+3] = (n1 >> 8) & 0xFF;
    msg[len+4] = n1 & 0xFF;

    // Send
    bool ret = VCPWrite(msg, len + 5);

    // Free
    free(msg);

    // Return
    return ret;
}

/**
 * @brief Write a debug message to the USB port with 2 trailing integers, using the DVMHost modem format
 * 
 * @param *text string to write (must be properly formatted string with null terminator)
 * @param n1 the first number to write
 * @param n2 the second number to write
 * 
 * @return true on success, false on error
*/
bool VCPWriteDebug3(const char *text, int16_t n1, int16_t n2)
{
    // Get length of string
    uint8_t len = strlen(text);

    // Return on invalid string
    if (!len)
    {
        return false;
    }

    // Allocate the array (+3 for header, +4 for 2 int)
    uint8_t *msg = malloc((len + 7) * sizeof *msg);

    // Make sure we alloc'd
    if (!msg)
    {
        return false;
    }

    // Populate header
    msg[0] = DVM_SHORT_FRAME_START;
    msg[1] = len + 7;
    msg[2] = CMD_DEBUG3;

    // Copy rest of message
    memcpy(msg + 3, text, len);

    // Copy the numbers
    msg[len+3] = (n1 >> 8) & 0xFF;
    msg[len+4] = n1 & 0xFF;
    msg[len+5] = (n2 >> 8) & 0xFF;
    msg[len+6] = n2 & 0xFF;

    // Send
    bool ret = VCPWrite(msg, len + 7);

    // Free
    free(msg);

    // Return
    return ret;
}

/**
 * @brief Write a debug message to the USB port with 3 trailing integers, using the DVMHost modem format
 * 
 * @param *text string to write (must be properly formatted string with null terminator)
 * @param n1 the first number to write
 * @param n2 the second number to write
 * @param n3 the third number to write
 * 
 * @return true on success, false on error
*/
bool VCPWriteDebug4(const char *text, int16_t n1, int16_t n2, int16_t n3)
{
    // Get length of string
    uint8_t len = strlen(text);

    // Return on invalid string
    if (!len)
    {
        return false;
    }

    // Allocate the array (+3 for header, +6 for 3 int)
    uint8_t *msg = malloc((len + 9) * sizeof *msg);

    // Make sure we alloc'd
    if (!msg)
    {
        return false;
    }

    // Populate header
    msg[0] = DVM_SHORT_FRAME_START;
    msg[1] = len + 9;
    msg[2] = CMD_DEBUG4;

    // Copy rest of message
    memcpy(msg + 3, text, len);

    // Copy the numbers
    msg[len+3] = (n1 >> 8) & 0xFF;
    msg[len+4] = n1 & 0xFF;
    msg[len+5] = (n2 >> 8) & 0xFF;
    msg[len+6] = n2 & 0xFF;
    msg[len+7] = (n3 >> 8) & 0xFF;
    msg[len+8] = n3 & 0xFF;

    // Send
    bool ret = VCPWrite(msg, len + 9);

    // Free
    free(msg);

    // Return
    return ret;
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
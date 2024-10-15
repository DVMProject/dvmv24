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
#include "log.h"
#include "fifo.h"
#include "util.h"
#include "config.h"
#include "string.h"
#include "hdlc.h"
#include "main.h"

#ifdef DVM_V24_V1
#include "usbd_cdc_if.h"
#else
#include "usart.h"
#include "stdlib.h"
#endif

// Indicates if the host has opened the port
extern bool USB_VCP_DTR;

// Flag indicating if we're in the process of receiveing a message
bool vcpRxMsgInProgress = false;

// Time we last received a byte
uint32_t vcpRxLastByte = 0;

// Flag that indicates we're expecting a double length message
bool vcpRxDoubleLength = false;

// Buffer for storing received message
uint8_t vcpRxMsg[VCP_MAX_MSG_LENGTH_BYTES];

// Expected total message length
uint16_t vcpRxMsgLength = 0U;

// Current message length
uint16_t vcpRxMsgPosition = 0U;

// VCP RX Fifo
uint8_t vcpRxBuf[VCP_RX_BUF_LEN];
FIFO_t vcpRxFifo = {
    .buffer = vcpRxBuf,
    .head = 0,
    .tail = 0,
    .maxlen = VCP_RX_BUF_LEN
};

// Buffer for tx message
uint8_t txBuffer[VCP_MAX_MSG_LENGTH_BYTES];
// TX message position/length
uint16_t txPos = 0U;

// VCP RX FIFO
uint8_t vcpTxBuf[VCP_TX_BUF_LEN];
FIFO_t vcpTxFifo = {
    .buffer = vcpTxBuf,
    .head = 0,
    .tail = 0,
    .maxlen = VCP_TX_BUF_LEN
};

// Vars for V2 serial implementation
#ifndef DVM_V24_V1
bool usartRx = false;
bool usartTx = false;
unsigned long usartTxStart = 0;
#define USART_RX_BUF_SIZE 1
uint8_t usartRxBuffer[USART_RX_BUF_SIZE] = {0};
#endif

#ifdef DVM_V24_V1

/**
 * @brief Called by the CDC_Receive_FS interrupt callback and fills the FIFO with the received bytes
*/
void VCPRxITCallback(uint8_t* buf, uint32_t len)
{
    uint32_t start = HAL_GetTick();
    for (uint32_t i = 0; i < len; i++)
    {
        if (FifoPush(&vcpRxFifo, buf[i]))
        {
            log_error("VCP RX FIFO full! Clearing buffer");
            // Clear
            FifoClear(&vcpRxFifo);
        }
    }
    if (HAL_GetTick() - start > FUNC_TIMER_WARN)
    {
        log_warn("VCPRxITCallback took %u ms!", HAL_GetTick() - start);
    }
    #ifdef TRACE_VCP_RX
    log_debug("Added %u bytes to VCP RX FIFO", len);
    #endif
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

#else

/**
 * @brief Interrupt handler for USART1 RX
 * 
 * @param huart uart to handle (should be usart1)
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    uint32_t start = HAL_GetTick();
    // Add received bytes to the fifo
    for (int i = 0; i < USART_RX_BUF_SIZE; i++)
    {
        if (FifoPush(&vcpRxFifo, usartRxBuffer[i]))
        {
            log_error("VCP RX FIFO full! Clearing buffer");
            FifoClear(&vcpRxFifo);
        }
    }
    if (HAL_GetTick() - start > FUNC_TIMER_WARN)
    {
        log_warn("HAL_UART_RxCpltCallback took %u ms!", HAL_GetTick() - start);
    }
    #ifdef TRACE_VCP_RX
    log_debug("Added %u bytes to VCP RX FIFO", USART_RX_BUF_SIZE);
    #endif
    // Call the interrupt again
    HAL_UART_Receive_DMA(huart, usartRxBuffer, USART_RX_BUF_SIZE);
}

/**
 * @brief Called by the HAL_UART_TxCpltCallback in serial.c
 */
void VCPTxComplete()
{
    // Check for excessive delay
    unsigned long txTime = HAL_GetTick() - usartTxStart;
    if (txTime > VCP_TX_TIMEOUT)
    {
        log_error("VCP USART TX routine took %u ms!", txTime);
    }
    // Reset buffer & position
    memset(txBuffer, 0x00U, VCP_MAX_MSG_LENGTH_BYTES);
    txPos = 0;
    // Reset LED
    LED_USB_TX(0);
    // Reset flag
    usartTx = false;
}

#endif

/**
 * @brief reset counters and flags related to VCP RX routine
*/
void vcpRxReset()
{
    vcpRxMsgInProgress = false;
    vcpRxDoubleLength = false;
    vcpRxMsgLength = 0U;
    vcpRxMsgPosition = 0U;
}

/**
 * @brief Called during main loop to handle any data received from USB
*/
void VCPRxCallback()
{
    /*
    
        The RX routine is basically copy-paste from dvmfirmware's Serialport::process() routine
    
    */

    #ifndef DVM_V24_V1
    if (!usartRx)
    {
        HAL_UART_Receive_DMA(&huart1, usartRxBuffer, USART_RX_BUF_SIZE);
        usartRx = true;
        log_info("Started USART1 RX DMA transfer");
    }
    #endif


    uint32_t start = HAL_GetTick();

    if (vcpRxFifo.size > 0)
    {
        #ifdef TRACE_VCP_RX
        log_debug("VCP RX FIFO size: %d/%d", vcpRxFifo.size, vcpRxFifo.maxlen);
        #endif
    }
    
    // Read data from the RX FIFO if available, until we process a full message, then break
    while (vcpRxFifo.size > 0)
    {
        // Turn activity LED on
        #ifdef DVM_V24_V1
        LED_USB(1);
        #else
        LED_USB_RX(1);
        #endif

        // Read a byte
        uint8_t c;
        if (FifoPop(&vcpRxFifo, &c))
        {
            log_warn("Tried to pop from empty FIFO, this shouldn't happen!");
            break;
        }

        vcpRxLastByte = HAL_GetTick();

        // If we're waiting for the start of a message, see if we got a message start byte
        if (vcpRxMsgPosition == 0U)
        {
            // Short frame length is a single byte
            if (c == DVM_SHORT_FRAME_START)
            {
                #ifdef DEBUG_VCP_RX
                log_debug("VCP RX: DVM_SHORT_FRAME_START");
                #endif
                // First byte of message set
                vcpRxMsg[0U] = c;
                // Increment our position pointer
                vcpRxMsgPosition = 1U;
                // Reset the length and double flag
                vcpRxMsgLength = 0U;
                vcpRxDoubleLength = false;
                // We're receiving a message
                vcpRxMsgInProgress = true;
            }
            // Long frame length is two bytes
            else if (c == DVM_LONG_FRAME_START)
            {
                #ifdef DEBUG_VCP_RX
                log_debug("VCP RX: DVM_LONG_FRAME_START");
                #endif
                vcpRxMsg[0U] = c;
                vcpRxMsgPosition = 1U;
                vcpRxMsgLength = 0U;
                vcpRxDoubleLength = true;
                // We're receiving a message
                vcpRxMsgInProgress = true;
            }
            // Anything else is ignored and we reset
            else
            {
                log_warn("Got invalid byte from VCP: %02X", c);
                vcpRxReset();
            }
        }

        // Receive length next
        else if (vcpRxMsgPosition == 1U)
        {
            // Handle double length
            if (vcpRxDoubleLength)
            {
                vcpRxMsg[vcpRxMsgPosition] = c;
                vcpRxMsgLength = ((c & 0xFFU) << 8);
            }
            // Handle normal length
            else
            {
                vcpRxMsgLength = vcpRxMsg[vcpRxMsgPosition] = c;
                #ifdef DEBUG_VCP_RX
                log_debug("VCP RX: Short message length: %d", vcpRxMsgLength);
                #endif
            }
            // Increment position
            vcpRxMsgPosition = 2U;
        }

        // Recieve second length byte if double length
        else if (vcpRxMsgPosition == 2U && vcpRxDoubleLength)
        {
            vcpRxMsg[vcpRxMsgPosition] = c;
            vcpRxMsgLength = (vcpRxMsgLength + (c & 0xFFU));
            vcpRxMsgPosition = 3U;
            #ifdef DEBUG_VCP_RX
            log_debug("VCP RX: Double message length: %d", vcpRxMsgLength);
            #endif
        }

        // Handle everything else
        else
        {
            // Make sure message length is valid
            if (vcpRxMsgLength > VCP_MAX_MSG_LENGTH_BYTES)
            {
                log_error("Message length %d is longer than supported!", VCP_MAX_MSG_LENGTH_BYTES);
                vcpRxReset();
            }
            // Add any other bytes to the buffer
            vcpRxMsg[vcpRxMsgPosition] = c;
            vcpRxMsgPosition++;

            // Process message if we've hit our length
            if (vcpRxMsgPosition == vcpRxMsgLength)
            {
                // This tells us where the command byte is
                uint8_t offset = 2U;
                if (vcpRxDoubleLength)
                {
                    offset = 3U;
                }

                #ifdef DEBUG_VCP_RX
                log_debug("VCP RX: Got DVM message, cmd: $%02X", vcpRxMsg[offset]);
                #endif

                // Process command
                switch (vcpRxMsg[offset])
                {
                    // Send P25 data as a UI frame
                    case CMD_P25_DATA:
                    {
                        // Process the UI frame
                        #ifdef DEBUG_VCP_RX
                        log_debug("Sending UI frame of length %d via HDLC", vcpRxMsgLength - offset - 2U);
                        #endif
                        #ifdef TRACE_VCP
                        uint8_t hexStrBuf[VCP_MAX_MSG_LENGTH_BYTES * 4U];
                        HexArrayToStr((char*)hexStrBuf, &vcpRxMsg[offset + 2U], vcpRxMsgLength - offset - 2U);
                        log_trace("P25 Frame: %s", hexStrBuf);
                        #endif
                        // Send the UI, ignoring the first 0x00 byte
                        HDLCSendUI(vcpRxMsg + offset + 2, vcpRxMsgLength - offset - 2);
                    }
                    break;
                    // Reply to version request
                    case CMD_GET_VERSION:
                        sendVersion();
                    break;
                    // Reply to status request
                    case CMD_GET_STATUS:
                        sendStatus();
                    break;
                    // Ignore config set command
                    case CMD_SET_CONFIG:
                        #ifdef DEBUG_VCP_RX
                        log_debug("Ignoring CMD_SET_CONFIG, no config to set");
                        #endif
                        VCPWriteAck(CMD_SET_CONFIG);
                    break;
                    // Ignore mode set command
                    case CMD_SET_MODE:
                        #ifdef DEBUG_VCP_RX
                        log_debug("Ignoring CMD_SET_MODE, always in P25 mode");
                        #endif
                    break;
                    // Ignore RF params config
                    case CMD_SET_RFPARAMS:
                        #ifdef DEBUG_VCP_RX
                        log_debug("Ignoring CMD_SET_RFPARAMS, no RF params to set");
                        #endif
                        VCPWriteAck(CMD_SET_RFPARAMS);
                    break;
                    case CMD_P25_CLEAR:
                        #ifdef DEBUG_VCP_RX
                        log_debug("Ignoring CMD_P25_CLEAR, not implemented yet");
                        #endif
                        // TODO: this
                        //log_info("Clearing and resetting synchronous serial");
                        //SyncReset();
                    break;
                    case CMD_CAL_DATA:
                        #ifdef DEBUG_VCP_RX
                        log_debug("Ignoring CMD_CAL_DATA and sending ACK");
                        #endif
                        VCPWriteAck(CMD_CAL_DATA);
                    break;
                    
                    // Flash read/write is only supported on DVM-V24-V2
                    #ifndef DVM_V24_V1
                    // Flash Read
                    case CMD_FLASH_READ:
                        #ifdef DEBUG_VCP_RX
                        log_debug("Reading flash to serial port");
                        #endif
                        flashRead();
                    break;
                    // Flash Write
                    case CMD_FLASH_WRITE:
                    {
                        #ifdef DEBUG_VCP_RX
                        log_debug("Writing data to flash from serial port");
                        #endif
                        uint8_t err = flashWrite(vcpRxMsg + 3U, vcpRxMsgLength - 3U);
                        if (err == RSN_OK)
                        {
                            VCPWriteAck(CMD_FLASH_WRITE);
                        }
                        else
                        {
                            log_error("Invalid flash data write: %u", err);
                            VCPWriteNak(CMD_FLASH_WRITE, err);
                        }
                    }
                    break;
                    #endif
                    // Reset MCU
                    case CMD_RESET_MCU:
                        ResetMCU();
                    break;
                    // Default handler
                    default:
                        log_warn("VCP RX: Unhandled DVM command %02X", vcpRxMsg[offset]);
                        VCPWriteNak(vcpRxMsg[offset], RSN_INVALID_REQUEST);
                    break;
                }

                // Reset
                vcpRxReset();

                #ifdef DEBUG_VCP_RX
                log_debug("VCP RX msg done!");
                /*if (vcpRxFifo.size > 0)
                {
                    log_debug("Remaining VCP RX FIFO size: %d/%d", vcpRxFifo.size, vcpRxFifo.maxlen);
                }*/
                #endif

                // Break from while loop so we can process TX
                break;
            }
        }

        // turn activity LED off
        #ifdef DVM_V24_V1
        LED_USB(0);
        #else
        LED_USB_RX(0);
        #endif
    }

    // Timeout and reset if we haven't received a full message
    if ((vcpRxMsgPosition > 0) && (HAL_GetTick() - vcpRxLastByte > VCP_RX_TIMEOUT))
    {
        log_error("Timed out waiting for full VCP message, resetting");
        vcpRxReset();
    }

    // Check elapsed time
    if (HAL_GetTick() - start > FUNC_TIMER_WARN)
    {
        log_warn("VCPCallback took %u ms!", HAL_GetTick() - start);
    }
}

/**
 * @brief VCP TX callback for handling outgoing messages to the host
 */
void VCPTxCallback()
{  
    #ifdef DVM_V24_V1
    // On V1, since we use USB, we don't have a CPLT callback and clear these here
    if (txPos > 0)
    {
        memset(txBuffer, 0x00U, VCP_MAX_MSG_LENGTH_BYTES);
        txPos = 0;
    }
    #else
    // On V2, everything is cleared by the complete callback so we just check if we're currently transmitting
    if (usartTx)
    {
        return;
    }
    usartTxStart = HAL_GetTick();
    #endif

    // Write any bytes in the VCP TX queue
    while (vcpTxFifo.size > 0)
    {
        // Read a byte
        uint8_t c;
        if (FifoPop(&vcpTxFifo, &c))
        {
            log_warn("Tried to pop from empty TX FIFO, this shouldn't happen!");
            break;
        }

        // Add to the buffer
        txBuffer[txPos] = c;
        txPos++;
    }

    // Return if we didn't get any bytes
    if (txPos == 0)
    {
        return;
    }

    #ifdef DVM_V24_V1
    
    // Directly write to the VCP
    bool sent = false;
    uint8_t attempts = USB_TX_RETRIES;
    while (attempts > 0)
    {
        // Break if USB port is closed
        if (!USB_VCP_DTR)
        {
            log_error("USB VCP disconnected, dropping message");
            return;
        }
        // Try to send
        LED_USB(1);
        // We have to disable the USB interrupts when using CDC_Transmit_FS, apparently
        HAL_NVIC_DisableIRQ(USB_LP_CAN1_RX0_IRQn);
        uint8_t rtn = CDC_Transmit_FS(txBuffer, txPos);
        HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
        attempts--;
        LED_USB(0);
        // Check for success
        if (rtn == USBD_OK)
        {
            sent = true;
            break;
        }
        else
        {
            log_warn("USB TX returned %d, retrying %d/%d", rtn, attempts, USB_TX_RETRIES);
        }
    }
    if (!sent)
    {
        log_error("Failed to write to USB port");
    }
    #else

    // Turn LED on (turned off by TX cplt callback)
    LED_USB_TX(1);

    // Set flag
    usartTx = true;

    // Write to USART1
    //HAL_UART_Transmit_DMA(&huart1, txBuffer, txPos);
    HAL_UART_Transmit(&huart1, txBuffer, txPos, VCP_TX_TIMEOUT);

    #ifdef DEBUG_VCP_TX
    log_debug("Sent %u-byte message to VCP TX DMA", txPos);
    #endif

    VCPTxComplete();

    #endif
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
    // Return false if the port isn't open
    #ifdef DVM_V24_V1
    if (!USB_VCP_DTR)
    {
        log_warn("USB VCP not open, dropping message");
        return false;
    }
    #endif

    // Add to TX fifo
    for (int i = 0; i < len; i++)
    {
        if (FifoPush(&vcpTxFifo, data[i]))
        {
            log_error("VCP TX FIFO full! Clearing");
            FifoClear(&vcpTxFifo);
            return false;
        }
    }

    return true;
}

/**
 * @brief Acknowledge a command
 * 
 * @param cmd command to ack
*/
bool VCPWriteAck(uint8_t cmd)
{
    uint8_t buffer[4U];

    buffer[0U] = DVM_SHORT_FRAME_START;
    buffer[1U] = 4U;
    buffer[2U] = CMD_ACK;
    buffer[3U] = cmd;

    return VCPWrite(buffer, 4U);
}

/**
 * @brief Send a NACK back
 * 
 * @param cmd 
 * @param err 
 * @return true 
 * @return false 
 */
bool VCPWriteNak(uint8_t cmd, uint8_t err)
{
    uint8_t buffer[5U];

    buffer[0U] = DVM_SHORT_FRAME_START;
    buffer[1U] = 5U;
    buffer[2U] = CMD_NAK;
    buffer[3U] = cmd;
    buffer[4U] = err;

    return VCPWrite(buffer, 5U);
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
    uint8_t buffer[VCP_MAX_MSG_LENGTH_BYTES];
    buffer[0] = DVM_SHORT_FRAME_START;
    buffer[1] = (uint8_t)len + 4;
    buffer[2] = CMD_P25_DATA;
    buffer[3] = 0x00;

    memcpy(buffer + 4, data, len);

    #ifdef DEBUG_VCP_TX
    log_debug("Writing P25 frame of length %d to VCP", len);
    #endif
    #ifdef TRACE_VCP
    uint8_t hexStrBuf[VCP_MAX_MSG_LENGTH_BYTES * 4];
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
    for (uint8_t i = 0U; i < (uint8_t)sizeof(HARDWARE_STRING); i++, count++)
        reply[count] = HARDWARE_STRING[i];

    // Total length
    reply[1U] = count;

    VCPWrite(reply, count);

    #ifdef DEBUG_VCP_TX
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

    // Report if V24 peer is connected or not
    if (HDLCPeerConnected)
    {
        reply[3U] |= 0x40U;
    }

    // Report P25 mode always
    reply[4U] = STATE_P25;

    // Calculate free space in the VCP RX FIFO
    #ifdef STATUS_SPACE_BLOCKS
    // Flag if we're reporting in 16-byte blocks
    reply[3U] |= 0x80U;
    uint8_t framesFree = (vcpRxFifo.maxlen - vcpRxFifo.size) / 16U;
    #else
    uint8_t framesFree = (vcpRxFifo.maxlen - vcpRxFifo.size) / P25_V24_LDU_FRAME_LENGTH_BYTES;  
    #endif
    
    // Reset buffers if we get low
    #ifdef STATUS_SPACE_BLOCKS
    if (framesFree < 16U)
    #else
    if (framesFree < 1U)
    #endif
    {
        log_error("TX buffer low: %d / %d bytes used, resetting buffer", vcpRxFifo.size, vcpRxFifo.maxlen);
        VCPWriteDebug3("TX buffer low, clearing. Bytes Free/Total: ", vcpRxFifo.size, vcpRxFifo.maxlen);
        FifoClear(&vcpRxFifo);
    }

    // Report free space in the VCP buffer (since that's the main one we care about)
    //reply[10U] = SyncGetTxFree();
    reply[10U] = framesFree;

    VCPWrite(reply, 15U);

    /*#ifdef TRACE_VCP
    log_trace("Sent DVM status information");
    #endif*/
}

#ifndef DVM_V24_V1

/**
 * @brief Read out the STM32 config flash page and send to the serial port
 * 
 * @return uint8_t 
 */
void flashRead()
{
    uint8_t reply[249U];
    reply[0U] = DVM_SHORT_FRAME_START;
    reply[1U] = 249U;
    reply[2U] = CMD_FLASH_READ;

    memcpy(reply + 3U, (void*)STM32_CNF_PAGE, 246U);

    VCPWrite(reply, 249U);
}

/**
 * @brief Write to STM32 config flash page
 * 
 * @param data data to write
 * @param length length of data to write
 * @return uint8_t return reason
 */
uint8_t flashWrite(const uint8_t* data, uint8_t length)
{
    if (length > 249U)
    {
        log_error("Flash write too big! %u > %u", length, 249U);
        return RSN_FLASH_WRITE_TOO_BIG;
    }

    // Unlock flash
    HAL_FLASH_Unlock();

    // Erase
    static FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t sectorError;
    EraseInitStruct.TypeErase   = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.PageAddress = STM32_CNF_PAGE_ADDR;
    EraseInitStruct.NbPages     = 1;
    HAL_StatusTypeDef status;
    status = HAL_FLASHEx_Erase(&EraseInitStruct, &sectorError);
    if (status != HAL_OK)
    {
        HAL_FLASH_Lock();
        log_error("HAL_FLASHEx_Erase failed with code %u", status);
        return RSN_FAILED_ERASE_FLASH;
    }

    // Compile bytes into words
    uint32_t address = STM32_CNF_PAGE_ADDR;
    uint8_t i = 0;
    while (i < length)
    {
        uint32_t word = 
            (uint32_t)(data[i + 3] << 24) +
            (uint32_t)(data[i + 2] << 16) +
            (uint32_t)(data[i + 1] << 8) +
            (uint32_t)(data);

        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, word);
        if (status != HAL_OK)
        {
            HAL_FLASH_Lock();
            log_error("HAL_FLASH_Program failed to write address %08X with code %u", address, status);
            return RSN_FAILED_WRITE_FLASH;
        }
        else
        {
            address += 4;
            i += 4;
        }
    }
    HAL_FLASH_Lock();
    return RSN_OK;
}

#endif

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
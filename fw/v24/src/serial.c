/**
  ******************************************************************************
  * @file           : serial.c
  * @brief          : Serial communication functions, similar to Arduino serial stuff
  ******************************************************************************
  */

// self-referential include
#include "serial.h"

#include "fifo.h"
#include "stdbool.h"
#include "config.h"

uint8_t serialDMABuffer[SERIAL_BUFFER_SIZE];

// Serial TX Fifo Buffer
uint8_t serialTxBuf[SERIAL_FIFO_SIZE];
FIFO_t serialTxFifo = {
    .buffer = serialTxBuf,
    .head = 0,
    .tail = 0,
    .maxlen = SERIAL_FIFO_SIZE
};

volatile bool serialTxSending = false;

/**
 * @brief fills the IT buffer with the next N bytes from the FIFO
 * 
 * @returns the number of bytes pulled from the FIFO
*/
uint16_t SerialFillDMA()
{
    // Get the next batch of bytes from the FIFO
    uint16_t bytes = 0;
    while (!FifoPop(&serialTxFifo, &serialDMABuffer[bytes]) && bytes < SERIAL_BUFFER_SIZE) { bytes++; }
    return bytes;
}

/**
 * @brief called in main loop to transmit serial non-blockingly
*/
void SerialCallback(UART_HandleTypeDef *huart)
{
    // Only start if we're not already running
    if (!serialTxSending)
    {
        uint16_t bytes = SerialFillDMA();
        if (bytes > 0)
        {
            serialTxSending = true;
            HAL_UART_Transmit_DMA(huart, serialDMABuffer, bytes);
        }
    }
}

/**
 * @brief called when the last IT UART transmit is complete
*/
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    uint16_t bytes = SerialFillDMA();
    if (bytes > 0)
    {
        HAL_UART_Transmit_DMA(huart, serialDMABuffer, bytes);
    }
    else
    {
        serialTxSending = false;
    }
}

/**
 *  @brief uart write command, writes bytes directly to terminal
 *  @param *huart: hardware UART object (pointer)
 *  @param *data: data array (pointer)
 *  @retval none
 */
void SerialWrite(UART_HandleTypeDef *huart, const char *data) {
    //HAL_UART_Transmit_IT(huart, (uint8_t*)data, strlen(data));
    uint16_t len = strlen(data);
    for (int i=0; i<len; i++)
    {
        if (FifoPush(&serialTxFifo, data[i]) == -1) {
            FifoClear(&serialTxFifo);
        }
    }
}

/**
 *  @brief uart write line command, adds a newline to the end of serialWrite
 *  @param *huart: hardware UART object (pointer)
 *  @param *data: data array (pointer)
 *  @retval none
 */
void SerialWriteLn(UART_HandleTypeDef *huart, const char *data) {
    SerialWrite(huart, data);
    SerialWrite(huart, "\r\n");
}

/**
 *  @brief Clears the serial terminal
*/
void SerialClear(UART_HandleTypeDef *huart) {
    SerialWrite(huart, "\033c");
}

/**
 *  @brief Prints the startup text to the serial console
 *  @param none
 *  @retval none
 */
void SerialStartup(UART_HandleTypeDef *huart) {
    SerialWriteLn(huart, W3AXL_LINE1);
    SerialWriteLn(huart, W3AXL_LINE2);
    SerialWriteLn(huart, W3AXL_LINE3);
    SerialWriteLn(huart, W3AXL_LINE4);
    SerialWriteLn(huart, W3AXL_LINE5);
    SerialWriteLn(huart, "");
    SerialWrite(huart, "    " PROG_NAME);
    SerialWrite(huart, " v");
    SerialWriteLn(huart, VERSION_STRING);
    SerialWriteLn(huart, "    " DATE_STRING);
    SerialWriteLn(huart, "");
}

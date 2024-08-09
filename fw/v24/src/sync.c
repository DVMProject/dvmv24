/**
  ******************************************************************************
  * @file           : sync.c
  * @brief          : Synchronous serial physical layer handling code abstraction
  ******************************************************************************
  */

// self-referential include
#include "sync.h"

// Libs
#include "leds.h"
#include "stdio.h"
#include "fifo.h"
#include "log.h"
#include "util.h"
#include "config.h"
#include "hdlc.h"
#include "vcp.h"

bool falling = true;
bool txd = false;
bool cts = true;

extern unsigned long rxTotalFrames;
extern unsigned long rxValidFrames;
extern unsigned long txTotalFrames;

// TX fifo buffer
uint8_t syncTxBuf[SYNC_TX_BUF_LEN];
FIFO_t syncTxFifo = {
    .buffer = syncTxBuf,
    .head = 0,
    .tail = 0,
    .maxlen = SYNC_TX_BUF_LEN
};
// Current byte to transmit
uint8_t syncTxByte = 0;
uint8_t syncTxBytePos = 0;
uint8_t syncLastTxByte = 0;
bool syncTxFlag = false;

// RX fifo buffer
uint8_t syncRxBuf[SYNC_RX_BUF_LEN];
FIFO_t syncRxFifo = {
    .buffer = syncRxBuf,
    .head = 0,
    .tail = 0,
    .maxlen = SYNC_RX_BUF_LEN
};
// Current byte being received
uint8_t rxCurrentByte = 0;
uint8_t rxBitCounter = 0;
uint8_t byteStuffed = 255;

// Status variables
enum RxState SyncRxState = SEARCH;
unsigned int SyncBytesReceived = 0;

// For detecting RX bit stuffing
uint8_t rxOnesCounter = 0;

// For TX bit stuffing
uint8_t txOnesCounter = 0;
uint8_t txLastBit = 0;

volatile bool rxMsgInProgress = false;

uint8_t rxCurMsg[SYNC_RX_BUF_LEN];
uint16_t rxCurPos = 0;
bool rxMsgStarted = false;
bool rxMsgComplete = false;

unsigned long syncRxTimer = 50; // timer for delay after sync reset/drop/startup

/**
 * @brief Starts the timer interrupt handler
 * @param *tim pointer to the timer
*/
void SyncStartup(TIM_HandleTypeDef *tim)
{
    HAL_TIM_Base_Start_IT(tim);
    log_info("Sync Serial Clocks Started");
}

void SyncResetRx()
{
    rxCurrentByte = 0;
    rxBitCounter = 0;
    SyncRxState = SEARCH;
    SyncBytesReceived = 0;
    rxCurPos = 0;
    rxMsgStarted = false;
    rxMsgComplete = false;
    FifoClear(&syncRxFifo);
    syncRxTimer = HAL_GetTick();
    log_info("Reset Sync RX");
}

void SyncDrop()
{
    LED_ACT(0);
    rxCurrentByte = 0;
    rxBitCounter = 0;
    SyncRxState = SEARCH;
    rxCurPos = 0;
    rxMsgStarted = false;
    rxMsgComplete = false;
    FifoClear(&syncRxFifo);
    HdlcReset();
    syncRxTimer = HAL_GetTick();
    log_error("Sync dropped");
}

/**
 * @brief Add byte to syncronous serial TX buffer
 * @param byte the byte to add
 * @return true on success, false on failure
*/
bool SyncAddTxByte(const uint8_t byte)
{
    if (FifoPush(&syncTxFifo, byte))
    {
        log_error("Sync TX buffer out of space!");
        return false;
    }
    return true;
}

bool SyncAddTxBytes(const uint8_t *bytes, int len)
{
    if (len > SYNC_TX_BUF_LEN)
    {
        log_error("Tried to add more TX bytes than TX buffer supports!");
        return false;
    }
    for (int i=0; i<len; i++)
    {
        if (!SyncAddTxByte(bytes[i]))
        {
            log_error("TX buffer ran out of space!");
            return false;
        }
    }
    return true;
}

void NextTxByte()
{
    // Reset flag
    syncTxFlag = false;
    
    // Check if we have data to send, and pop until we don't
    if (FifoPop(&syncTxFifo, &syncTxByte))
    {
        syncTxByte = HDLC_SYNC_WORD;
        LED_ACT(0);
    }
    else
    {
        LED_ACT(1);
    }

    // If we got a true flag, note it
    if (syncTxByte == HDLC_SYNC_WORD) { syncTxFlag = true; }
    // Unescape things if needed
    if (syncTxByte == HDLC_ESCAPE_CODE)
    {
        // Figure out the escaped byte
        if (FifoPop(&syncTxFifo, &syncTxByte))
        {
            log_error("Got escape character but nothing following!");
            return;
        }
        // Escape the 0x7D
        if (syncTxByte == HDLC_ESCAPE_7D)
        {
            syncTxByte = 0x7D;
        }
        // Escape the 0x7E and note that it's not a flag
        else if (syncTxByte == HDLC_ESCAPE_7E)
        {
            syncTxByte = 0x7E; 
            syncTxFlag = false;
        }
    }
}

bool NextTxBit()
{
    // Check for stuffing first
    if (txOnesCounter == 5 && !syncTxFlag)
    {
        // Reset the counter
        txOnesCounter = 0;
        // Don't increment the position counter and return an extra 0
        return 0;
    }
    else
    {
        // Increment the bit in our current byte
        syncTxBytePos++;
        // Get the next byte if we've done all 8 bits
        if (syncTxBytePos >= 8)
        {
            syncTxBytePos = 0;
            NextTxByte();
        }
        // Get new bit
        txLastBit = GetBitAtPos(syncTxByte, syncTxBytePos);
        // If it's a 1, increment our ones counter
        if (txLastBit) {
            txOnesCounter++;
        // If it's a zero, reset our ones counter
        } else {
            txOnesCounter = 0;
        }
        return txLastBit;
    }
}

/**
 * @brief called from main loop and tries to parse bytes from the fifo into messages
*/
void RxMessageCallback()
{
    // Do nothing without sync
    if (SyncRxState != SYNCED) {
        return;
    }
    // Pop bytes from Fifo until the next sync word
    uint8_t newByte = 0;
    while (!FifoPop(&syncRxFifo, &newByte) && !rxMsgComplete)
    {
        LED_ACT(1);
        #ifdef TRACE_SYNC
        log_trace("Popped %02X from RX FIFO",newByte);
        #endif
        // Check for two repeating sync words which delineate 
        if (newByte == HDLC_SYNC_WORD)
        {
            if (!rxMsgStarted) {
                rxMsgStarted = true;
            }
            else
            {
                rxMsgComplete = true;
            }
        }
        else
        {
            if (rxMsgStarted)
            {
                rxCurMsg[rxCurPos] = newByte;
                rxCurPos++;
            }
        }
    }
    // Process the complete message
    if (rxMsgComplete)
    {
        if (rxCurPos > 1)
        {
            // If we fail to parse the message, drop sync
            if (HDLCParseMsg(rxCurMsg, rxCurPos)) // minus 1 to remove the trailing flag (TODO: make it smarter)
            {
                SyncDrop();
            }
        }
        
        // Reset
        rxCurPos = 0;
        rxMsgStarted = false;
        rxMsgComplete = false;
        LED_ACT(0);
    }
}

void RxBits()
{
    // Wait for timeout to clear
    if (HAL_GetTick() - syncRxTimer < SYNC_RX_DELAY) {
        return;
    // 0 is our "done" state so we only print the log message once
    } else if (syncRxTimer > 0) {
        log_info("Sync RX starting");
        syncRxTimer = 0;
    }
    // Read the state of each RX pin
    bool rxd = GET_RXD();
    //bool rts = GET_RTS();
    // Shift the latest RX bit into the byte buffer (from the left since we receive bits LSB-first)
    rxCurrentByte = (rxCurrentByte >> 1) | (rxd << 7);
    //log_trace("RX state: %d, Current RX byte: %02X ("BYTE_TO_BINARY_PATTERN")", SyncRxState, rxCurrentByte, BYTE_TO_BINARY(rxCurrentByte));
    // Do different things depending on state
    switch (SyncRxState)
    {
        // searching for the HDLC sync frame
        case SEARCH:      
            // Check what's currently in there
            if (rxCurrentByte == HDLC_SYNC_WORD)
            {
                // Switch state to synced and reset the current byte
                SyncRxState = SYNCED;
                log_info("HDLC RX now synced");
                rxCurrentByte = 0;
                rxBitCounter = 0;
            }
            break;
        
        // If we're synced, process bits & bytes like normal
        case SYNCED:
            // If we've received 5 1s and the next bit is 0, that's a stuffed bit and we should ignore
            if (rxOnesCounter == 5 && rxd == 0)
            {
                //log_debug("Got a stuffed bit, skipping");
                // note the location of the stuffed bit (important to differentiate a flag below)
                byteStuffed = rxBitCounter;
                // Reset the counter
                rxOnesCounter = 0;
                // shift over 1 (dropping the last bit received) and dont increment the bit counter
                rxCurrentByte <<= 1;
            }
            // If we've received 6 1s and the next bit is also a 1, that can't happen normally and we should drop sync
            else if (rxOnesCounter == 6 && rxd == 1)
            {
                log_error("Received 7 consecutive 1s, this is bad, dropping sync!");
                SyncDrop();
            }
            else
            {
                // Increment the ones counter if needed
                if (rxd) { rxOnesCounter += 1; } else { rxOnesCounter = 0; }
                
                // Increment the bit counter
                rxBitCounter++;
                // If we've received 8 bits, push the byte to the Fifo and reset
                if (rxBitCounter == 8)
                {
                    if (rxCurrentByte == HDLC_SYNC_WORD) {
                        // If we got a sync word, check to see if there was a message being sent
                        if (rxMsgInProgress)
                        {
                            // If the byte was stuffed at position 6, it's not actually a sync word so escape it
                            if (byteStuffed == 6)
                            {
                                uint8_t res = 0;
                                res += FifoPush(&syncRxFifo, HDLC_ESCAPE_CODE);
                                res += FifoPush(&syncRxFifo, HDLC_ESCAPE_7E);
                                if (res != 0)
                                {
                                    log_warn("syncRxFifo full!");
                                }
                            }
                            else
                            {
                                // Message is done
                                rxMsgInProgress = false;
                                // Append tailing sync word so RxCallback can find the end
                                if (FifoPush(&syncRxFifo, HDLC_SYNC_WORD))
                                {
                                    log_warn("syncRxFifo full!");
                                }
                            }
                        }
                    }
                    // If the byte isn't a sync word, put it in the fifo
                    else
                    {
                        // message now in progress
                        if (!rxMsgInProgress)
                        {
                            // Set flag
                            rxMsgInProgress = true;
                            // Push starting sync word so RxCallback can find the start
                            if (FifoPush(&syncRxFifo, HDLC_SYNC_WORD))
                            {
                                log_warn("syncRxFifo full!");
                            }
                        }
                        // Escape 0x7D if it comes up
                        if (rxCurrentByte == 0x7D)
                        {
                            uint8_t res = 0;
                            res += FifoPush(&syncRxFifo, HDLC_ESCAPE_CODE);
                            res += FifoPush(&syncRxFifo, HDLC_ESCAPE_7D);
                            if (res != 0)
                            {
                                log_warn("syncRxFifo full!");
                            }
                        }
                        else
                        {
                            if (FifoPush(&syncRxFifo, rxCurrentByte))
                            {
                                log_warn("syncRxFifo full!");
                            }
                        }
                    }
                    // Reset everything
                    byteStuffed = 255;
                    rxCurrentByte = 0;
                    rxBitCounter = 0;
                    
                }
                else if (rxBitCounter > 8)
                {
                    log_error("RX bit counter exceeded, dropping sync");
                    SyncDrop();
                }
            }
            
        break;

        default:
            log_error("RX sync state machine got invalid state %d", SyncRxState);
        break;
    }
}

/**
 * @brief Callback for the 9600 baud timer (actually runs at X2 speed), clocks data in & out
*/
void SyncTimerCallback(void)
{
    // We do serial stuff on the rising edge of the clock.
    if (falling)
    {
        falling = false;
        // Set clocks high before we do anything
        TXCLK_HIGH();
        // Set TX pins to their proper state
        SET_TXD(NextTxBit());
        //SET_CTS(cts);
        // Read RX pins
        RxBits();
    }
    // On the falling edge we do nothing
    else
    {
        falling = true;
        // Gate the TX & RX pins
        TXCLK_LOW();
    }
}

/**
 * @brief Report the free space in the fifo, divided by the LDU frame size in bytes
*/
uint8_t SyncGetTxFree()
{
    uint16_t framesFree = (syncTxFifo.maxlen - syncTxFifo.size) / P25_LDU_FRAME_LENGTH_BYTES;
    // Reset buffers if we get low
    if (framesFree < 1)
    {
        log_error("TX buffer low: %d / %d bytes used, resetting buffer", syncTxFifo.size, syncTxFifo.maxlen);
        VCPWriteDebug3("TX buffer low, clearing. Free/Total: ", syncTxFifo.size, syncTxFifo.maxlen);
        FifoClear(&syncTxFifo);
    }
    return framesFree;
}
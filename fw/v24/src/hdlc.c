/**
  ******************************************************************************
  * @file           : HDLC.c
  * @brief          : HDLC framing utilities
  ******************************************************************************
  */

// self-referential include
#include "hdlc.h"

#include "sync.h"
#include "stdio.h"
#include "string.h"
#include "util.h"
#include "vcp.h"

extern unsigned long rxValidFrames;
extern unsigned long rxTotalFrames;
extern unsigned long txTotalFrames;

extern enum RxState SyncRxState;

// Timers for various events
unsigned long hdlcLastRx = 0;
unsigned long hdlcLastTx = 0;
unsigned long lastStatus = 0;
unsigned long lastUIFrame = 0;

// Address of the V24 peer (quantar), received from first SABM
uint8_t peerAddress = 0x0;

bool HDLCPeerConnected = false;

/**
 * @brief Calculate FCS for data array
 * @param *data pointer to data array
 * @param crc seed for CRC generation
 * @param len length of data array
 * @return FCS value, properly reversed for HDLC order
 * 
 * adapted from 
 * https://stackoverflow.com/questions/59966346/how-should-i-implement-crc-for-hdlc-crc16-ccitt
 * 
 * Handy calculator for reference:
 * http://www.sunshine2k.de/coding/javascript/crc/crc_js.html
 * (use CRC16_X_25 preset)
*/
uint16_t crc16(const uint8_t *data, uint8_t len)
{
    uint16_t crc = 0xFFFF;
    while (len--)
    {
        crc = (crc >> 8) ^ crcTable[(crc^*data++)&0xFF];
    }
    crc ^= 0xFFFF;
    return crc;
}

void HdlcReset()
{
    if (HDLCPeerConnected)
    {
        HDLCPeerConnected = false;
        log_info("HDLC reset");
    }
}

/**
 * @brief called every loop of main, handles HDLC timers
*/
void HdlcCallback()
{
    if (SyncRxState == SYNCED)
    {
        // RX timeout handler to reset if we haven't received a message in time
        if (HAL_GetTick() - hdlcLastRx > RX_TIMEOUT)
        {
            log_error("HDLC RX timeout, dropping sync!");
            SyncDrop();
            hdlcLastRx = HAL_GetTick();
        }
        
        // Only send RR if we've got a peer
        if (HDLCPeerConnected)
        {
            // RR TX interval handler
            if (HAL_GetTick() - hdlcLastTx > RR_INTERVAL)
            {
                HDLCSendRR();
            }
        }
    }
    
    // Periodic status print
    if (HAL_GetTick() - lastStatus > STATUS_INTERVAL)
    {
        lastStatus = HAL_GetTick();
        #ifdef PERIODIC_STATUS
        if (HDLCPeerConnected)
        {
            log_info("V24 peer connected. Frames: [RX: %d, TX: %d, ER: %d]", rxValidFrames, txTotalFrames, rxTotalFrames - rxValidFrames);   
        }
        else if (SyncRxState == SYNCED)
        {
            log_info("HDLC synced, waiting for peer. Frames: [RX: %d, TX: %d, ER: %d]", rxValidFrames, txTotalFrames, rxTotalFrames - rxValidFrames);
        }
        else if (SyncRxState == SEARCH)
        {
            log_warn("HDLC frame sync lost");
        }
        #endif
    }
}

void hdlcFrameSpace()
{
    for (int i=0; i<FRAME_SPACING; i++)
    {
        SyncAddTxByte(HDLC_SYNC_WORD);
    }
}

/**
 * @brief Encodes, escapes, and sends an HDLC frame
 * 
 * outputData length should be +2 from input data to allow for trailing FCS
 * 
 * @param outputData output buffer to store encoded frame in
 * @param inputData input data array to encode
 * @param inputLength length of input data array
*/
void hdlcEncodeAndSendFrame(const uint8_t *data, const uint8_t len)
{
    // Calculate FCS of message
    uint16_t fcs = crc16(data, len);
    // Append
    uint8_t frame[len + 2];
    memcpy(&frame, data, len);
    frame[len] = low(fcs);
    frame[len + 1] = high(fcs);
    // Trace
    #ifdef DEBUG_HDLC
    uint8_t hexStrBuf[(len + 2)*4];
    HexArrayToStr((char*)hexStrBuf, frame, len + 2);
    log_trace("Encoded HDLC: %s", hexStrBuf);
    #endif
    // See if we need to escape anything
    uint8_t escapes = HDLCGetEscapesReq(frame, len + 2);
    if (escapes)
    {
        uint8_t escFrame[len + 2 + escapes];
        if (HDLCEscape(escFrame, frame, len + 2) != escapes)
        {
            log_error("Didn't escape the bytes we expected!");
        }
        else
        {
            // Add the message and a trailing 7E
            if (SyncAddTxBytes(escFrame, len + 2 + escapes))
            {
                txTotalFrames++;
                hdlcFrameSpace();
            }
        }
    }
    else
    {
        // Add the message and a trailing 7E
        if (SyncAddTxBytes(frame, len + 2))
        {
            hdlcFrameSpace();
            txTotalFrames ++;
        }
        
    }
    // Update timer
    hdlcLastTx = HAL_GetTick();
}

/**
 * @brief Send SABM (Set Asynchronous Balanced Mode) message to address
 * @param address address to send
*/
void HDLCSendSABM(uint8_t address)
{
    const uint8_t data[2] = { address, HDLC_CTRL_SABM };
    hdlcEncodeAndSendFrame(data, 2);
    log_info("Sent SABM frame");
}

void HDLCSendUA(uint8_t address)
{
    const uint8_t data[2] = { address, HDLC_CTRL_UA };
    hdlcEncodeAndSendFrame(data, 2);
    log_info("Sent UA frame");
}

void HDLCSendXID(uint8_t address, uint8_t msg_type, uint8_t site, uint8_t station_type)
{
    const uint8_t data[10] = { address, HDLC_CTRL_XID, msg_type, (site * 2) + 1, station_type, 0, 0, 0, 0, 0xFF };
    hdlcEncodeAndSendFrame(data, 10);
    log_info("Sent XID frame");
}

void HDLCSendRR()
{
    const uint8_t data[2] = { HDLC_ADDRESS, 0x01 };
    hdlcEncodeAndSendFrame(data, 2);
    log_info("Sent RR frame");
}

void HDLCSendUI(uint8_t *msgData, uint8_t len)
{
    // We need 2 extra bytes for address and control
    uint8_t data[len + 2];
    data[0] = peerAddress;
    data[1] = HDLC_CTRL_UI;
    memcpy(&data[2], msgData, len);
    // Encode frame
    hdlcEncodeAndSendFrame(data, len + 2);
    #ifdef INFO_HDLC
    log_info("Sent UI frame");
    #endif
}

/**
 * @brief Checks an HDLC message for correct FCS
 * 
 * @param msg pointer to HDLC message
 * @param len length of HDLC message
 * 
 * @return true on good FCS, false if not
*/
bool HDLCCheckFCS(uint8_t *msg, uint8_t len)
{
    // Get received FCS from last two bytes of message
    uint16_t msg_fcs = (msg[len-1]<<8) | msg[len-2];
    // Calculate FCS for all bytes except the last two
    uint16_t calc_fcs = crc16(msg, len-2);
    // Return
    if (msg_fcs != calc_fcs)
    {
        log_error("Expected FCS %04X, Got FCS %04X", calc_fcs, msg_fcs);
        return false;
    }
    else
    {
        return true;
    }
}

/**
 * @brief counts the number of 0x7E or 0x7D bytes in a message, for escaping purposes
 * 
 * @param *msg input message
 * @param len input message length
 * 
 * @returns the number of 0x7E and 0x7D bytes
*/
uint8_t HDLCGetEscapesReq(uint8_t *msg, uint8_t len)
{
    // Count the number of 7E and 7D bytes in the original message
    uint8_t numEscapes = 0;
    for (int i=0; i<len; i++)
    {
        if (msg[i] == 0x7E || msg[i] == 0x7D)
        {
            numEscapes++;
        }
    }
    return numEscapes;
}

/**
 * @brief Parse any values that need escaping (0x7E or 0x7D) and escape them
 * 
 * @param *out output buffer to write the unescaped data to (must be big enough!)
 * @param *msg input data to escape
 * @param len input data length
 * 
 * @returns the number of escape sequences
*/
uint8_t HDLCEscape(uint8_t *out, uint8_t *msg, uint8_t len)
{
    uint8_t numEscapes = 0;
    for (int i=0; i<len; i++)
    {
        if (msg[i] == 0x7E)
        {
            out[i+numEscapes] = 0x7D;
            out[i+numEscapes+1] = 0x5E;
            numEscapes++;
        }
        else if (msg[i] == 0x7D)
        {
            out[i+numEscapes] = 0x7D;
            out[i+numEscapes+1] = 0x5D;
            numEscapes++;
        }
        else
        {
            out[i+numEscapes] = msg[i];
        }
    }
    return numEscapes;
}

/**
 * @brief Parse any escape sequences back to their original values
 * 
 * @param *out output buffer to write the unescaped data to
 * @param *msg input data to escape
 * @param len length of input data
 * 
 * @returns the number of escape sequences
*/
uint8_t HDLCUnescape(uint8_t *out, uint8_t *msg, uint8_t len)
{
    uint8_t lastByte = 0;
    uint8_t numEscapes = 0;
    for (int i=0; i<len; i++)
    {
        if (lastByte == HDLC_ESCAPE_CODE && msg[i] == HDLC_ESCAPE_7E)
        {
        	numEscapes++;
            out[i-numEscapes] = 0x7E;
            #ifdef TRACE_HDLC
            log_trace("Replaced 7D 5E with 7E");
            #endif
        }
        else if (lastByte == HDLC_ESCAPE_CODE && msg[i] == HDLC_ESCAPE_7D)
        {
        	numEscapes++;
        	out[i-numEscapes] = 0x7D;
            #ifdef TRACE_HDLC
            log_trace("Replaced 7D 5D with 7D");
            #endif
        }
        else
        {
            out[i-numEscapes] = msg[i];
            #ifdef TRACE_HDLC
            log_trace("msg[%d] (%02X) -> out[%d] (%02X)", i, msg[i], i-numEscapes, out[i-numEscapes]);
            #endif
        }
        lastByte = msg[i];
    }
    return numEscapes;
}

/**
 * @brief Processes a full HDLC message (exclusing sync words)
 * 
 * @returns 1 on error, 0 on success
*/
uint8_t HDLCParseMsg(uint8_t* rawMsg, uint8_t rawLen)
{
    // Debug print hex buffer
    #if defined(DEBUG_HDLC) || defined(TRACE_HDLC)
    uint8_t hexStrBuf[rawLen * 4];
    #endif

    // Incremenet total frames processed
    rxTotalFrames++;
    // Log print
    #ifdef DEBUG_HDLC
    printHexArray((char*)hexStrBuf, rawMsg, rawLen);
    log_debug("Processing %d-byte HDLC message:%s", rawLen, hexStrBuf);
    #endif
    // Escape
    uint8_t msg[rawLen];
    uint8_t len = rawLen - HDLCUnescape(msg, rawMsg, rawLen);
    #ifdef DEBUG_HDLC
    if (len != rawLen)
    {
        printHexArray((char*)hexStrBuf, msg, len);
        log_debug("Escaped %d bytes to new %d-byte message:%s", rawLen - len, len, hexStrBuf);
    }
    #endif
    // Get parameters
    uint8_t msg_addr = msg[0];
    uint8_t msg_ctrl = msg[1];
    // Data is bytes 2 to len-2 so buffer size is 4 less
    uint8_t data_len = len - 4;
    uint8_t msg_data[data_len];
    memset(msg_data, 0, data_len);
    memcpy(msg_data, &msg[2], (data_len) * sizeof(uint8_t));
    #ifdef TRACE_HDLC
    printHexArray((char*)hexStrBuf, msg_data, data_len);
    log_trace("Msg data:%s", hexStrBuf);
    #endif
    // Calculate expected FCS by getting the entire message minus the FCS
    if (!HDLCCheckFCS(msg, len))
    {
        log_error("FCS check failed!");
        #ifdef TRACE_HDLC
        printHexArray((char*)hexStrBuf, msg, len);
        log_trace("Message:%s", hexStrBuf);
        #endif
        return 1;
    }
    // Increment valid frames counter
    rxValidFrames++;
    // Update the peer address if needed
    if (!peerAddress) {
        peerAddress = msg_addr;
        log_info("Got HDLC peer address %02X", peerAddress);
    }
    // Handle the message
    switch (msg_ctrl)
    {
        // We respond to an SABM with a UA
        case HDLC_CTRL_SABM:
            log_info("Got SABM frame");
            hdlcLastRx = HAL_GetTick();
            HDLCSendUA(peerAddress);
            break;
        // We respond to an XID by storing the data and sending our own
        case HDLC_CTRL_XID:
            log_info("Got XID frame");
            hdlcLastRx = HAL_GetTick();
            HDLCSendXID(HDLC_ADDRESS, HDLC_CTRL_XID, HDLC_SITE, 0x00);
            break;
        case HDLC_CTRL_RR:
            log_info("Got RR frame");
            hdlcLastRx = HAL_GetTick();
            if (!HDLCPeerConnected)
            {
                log_info("Connected to HDLC peer %02X", peerAddress);
                HDLCPeerConnected = true;
            }
            break;
        case HDLC_CTRL_UI:
            #ifdef INFO_HDLC
            log_info("Got UI frame");
            #endif
            hdlcLastRx = HAL_GetTick();
            // Write frame to VCP
            VCPWriteP25Frame(msg_data, data_len);
            break;
        default:
            log_warn("Unhandled HDLC control type %02X", msg_ctrl);
            break;
    }
    return 0;
}

/**
 ******************************************************************************
 * @file           : util.c
 * @brief          : Misc utility functions
 ******************************************************************************
 */

/* Self referential incude */
#include "util.h"

/**
 * @brief Converts an array of chars to a space-separated string with braces
 * @param *buf pointer to output string buffer
 * @param *array pointer to array to print
 * @param len length of array
 */
void HexArrayToStr(char *buf, uint8_t *array, uint8_t len)
{
    sprintf(buf, "{ ");
    for (uint8_t i = 0; i < len; i++)
    {
        sprintf(buf + strlen(buf), "%02X ", array[i]);
    }
    sprintf(buf + strlen(buf), "}");
}

/**
 * @brief get bit value at position in byte
 * @param byte the byte to query
 * @param pos the position (from the right)
 * @return the value of the bit (0 or 1)
 */
bool GetBitAtPos(uint8_t byte, uint8_t pos)
{
    return (byte & (1 << pos)) >> pos;
}

/**
 * @brief sets bit at position to value
 * @param byte the byte to change
 * @param pos the position (from the right)
 * @param value the value to set (0 or 1)
 * @return the new byte
 */
uint8_t SetBitAtPos(uint8_t byte, uint8_t pos, bool value)
{
    uint8_t mask = 1 << pos;
    return ((byte & ~mask) | (value << pos));
}

/**
 * @brief Prints an array of hex characters, space separated
 * @param outBuf output buffer (must be 3x the length of input array)
 * @param array input array to print
 * @param len length of array
 */
void printHexArray(char *outBuf, uint8_t *array, uint8_t len)
{
    for (uint8_t i = 0; i < len; i++)
    {
        sprintf(outBuf + (i * 3), " %02X", array[i]);
    }
}

/**
 * @brief Flips the bits in a byte (used to convert from LSB to MSB, etc)
 *
 * borrowed from
 * https://stackoverflow.com/questions/2602823/in-c-c-whats-the-simplest-way-to-reverse-the-order-of-bits-in-a-byte
 */
unsigned char reverseBits(unsigned char b)
{
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}
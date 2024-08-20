/**
  ******************************************************************************
  * @file           : fifo.c
  * @brief          : Simple implementation of a FIFO in c
  * 
  * This was created from the guide at
  * https://embedjournal.com/implementing-circular-buffer-embedded-c/
  ******************************************************************************
  */

// self-referential include
#include "fifo.h"

/**
 * @brief Pushes data into the end of the fifo
 * @param *c fifo pointer
 * @param data data to store
 * @return 0 on success, -1 if fifo full
*/
int FifoPush(FIFO_t *c, uint8_t data)
{
    int next;
    next = c->head + 1;
    if (next >= c->maxlen)
    {
        next = 0;
    }

    if (next == c->tail)
    {
        return -1;
    }

    c->buffer[c->head] = data;
    c->head = next;

    // Update our size
    if (c->size < c->maxlen) {
        c->size++;
    } else {
        return -1;
    }

    return 0;
}

/**
 * @brief Pops the first item from the fifo
 * @param *c fifo pointer
 * @param *data where to store popped data
 * @return 0 on success, -1 if fifo empty
*/
int FifoPop(FIFO_t *c, uint8_t *data)
{
    int next;

    if (c->head == c->tail)
    {
        return -1;
    }

    // Figure out where the new tail will be
    next = c->tail + 1;
    if (next >= c->maxlen)
    {
        next = 0;
    }

    // Get the data at the current tail
    *data = c->buffer[c->tail];
    
    // Update the tail position
    c->tail = next;
    
    // Update our size
    if (c->size > 0) {
        c->size--;
    } else {
        // We somehow got data from a 0-space FIFO
        return -1;
    }

    return 0;
}

int FifoPeek(FIFO_t *c, uint8_t *data)
{
    // If we're empty, return -1
    if (c->head == c->tail)
    {
        return -1;
    }
    // Return the value of the next data item without popping it
    *data = c->buffer[c->tail];
    return 0;
}

void FifoClear(FIFO_t *c)
{
    uint8_t data = 0;
    while (FifoPop(c, &data) != -1) {}
    c->size = 0;
    return;
}
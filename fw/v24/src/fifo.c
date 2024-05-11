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

    next = c->tail + 1;
    if (next >= c->maxlen)
    {
        next = 0;
    }

    *data = c->buffer[c->tail];
    c->tail = next;
    return 0;
}

void FifoClear(FIFO_t *c)
{
    uint8_t data = 0;
    while (FifoPop(c, &data) != -1) {}
    return;
}
/**
 ******************************************************************************
 * @file           : fault.c
 * @brief          : Fault handling utilities for STM32
 *
 * This was created from the guide at
 * https://rhye.org/post/stm32-with-opencm3-5-fault-handlers/
 ******************************************************************************
 */

#include "fault.h"
#include "usart.h"
#include "core_cm3.h"
#include <stdio.h>

static const char *system_interrupt_names[16] = {
    "SP_Main", "Reset", "NMI", "Hard Fault",
    "MemManage", "BusFault", "UsageFault", "Reserved",
    "Reserved", "Reserved", "Reserved", "SVCall",
    "DebugMonitor", "Reserved", "PendSV", "SysTick"};

void fault_handler_printf(const char * msg)
{
    uint16_t len = strlen(msg);
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, len, 100);
}

enum { r0, r1, r2, r3, r12, lr, pc, psr };
void dump_registers(uint32_t stack[]) {
  static char msg[32];
  sprintf(msg, "r0  = 0x%lx\n", stack[r0]);
  fault_handler_printf(msg);
  sprintf(msg, "r1  = 0x%lx\n", stack[r1]);
  fault_handler_printf(msg);
  sprintf(msg, "r2  = 0x%lx\n", stack[r2]);
  fault_handler_printf(msg);
  sprintf(msg, "r3  = 0x%lx\n", stack[r3]);
  fault_handler_printf(msg);
  sprintf(msg, "r12 = 0x%lx\n", stack[r12]);
  fault_handler_printf(msg);
  sprintf(msg, "lr  = 0x%lx\n", stack[lr]);
  fault_handler_printf(msg);
  sprintf(msg, "pc  = 0x%lx\n", stack[pc]);
  fault_handler_printf(msg);
  sprintf(msg, "psr = 0x%lx\n", stack[psr]);
  fault_handler_printf(msg);
}

void base_fault_handler(uint32_t stack[])
{
    // The implementation of these fault handler printf methods will depend on
    // how you have set your microcontroller up for debugging - they can either
    // be semihosting instructions, write data to ITM stimulus ports if you
    // are using a CPU that supports TRACESWO, or maybe write to a dedicated
    // debug UART
    fault_handler_printf("Fault encountered!\n");
    static char buf[64];
    // Get the fault cause. Volatile to prevent compiler elision.
    const volatile uint8_t active_interrupt = SCB->ICSR & 0xFF;
    // Interrupt numbers below 16 are core system interrupts, we know their names
    if (active_interrupt < 16)
    {
        sprintf(buf, "Cause: %s (%u)\n", system_interrupt_names[active_interrupt],
                 active_interrupt);
    }
    else
    {
        // External (user) interrupt. Must be looked up in the datasheet specific
        // to this processor / microcontroller.
        sprintf(buf, "Unimplemented user interrupt %u\n", active_interrupt - 16);
    }
    fault_handler_printf(buf);

    fault_handler_printf("Saved register state:\n");
    dump_registers(stack);
    __asm volatile("BKPT #01");
    while (1)
    {
    }
}
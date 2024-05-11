#ifndef LEDS_H
#define LEDS_H

// LED Command Defines
#define LED_HB(state)       HAL_GPIO_WritePin(LED_HB_GPIO_Port, LED_HB_Pin, state)
#define LED_USB(state)      HAL_GPIO_WritePin(LED_USB_GPIO_Port, LED_USB_Pin, state)
#define LED_LINK(state)     HAL_GPIO_WritePin(LED_LINK_GPIO_Port, LED_LINK_Pin, state)
#define LED_ACT(state)      HAL_GPIO_WritePin(LED_ACT_GPIO_Port, LED_ACT_Pin, state)

// Miscellaneous Parameters
#define LED_TEST_DURATION   5000

#endif
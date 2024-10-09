#ifndef LEDS_H
#define LEDS_H

// LED Command Defines
#define LED_LINK(state)     HAL_GPIO_WritePin(LED_LINK_GPIO_Port, LED_LINK_Pin, state)
#define LED_ACT(state)      HAL_GPIO_WritePin(LED_ACT_GPIO_Port, LED_ACT_Pin, state)

#ifdef DVM_V24_V1
#define LED_HB(state)       HAL_GPIO_WritePin(LED_HB_GPIO_Port, LED_HB_Pin, state)
#define LED_USB(state)      HAL_GPIO_WritePin(LED_USB_GPIO_Port, LED_USB_Pin, state)
#else
#define LED_USB_TX(state)   HAL_GPIO_WritePin(LED_USB_GPIO_Port, LED_USB_Pin, state)
#define LED_USB_RX(state)   HAL_GPIO_WritePin(USB_ENUM_GPIO_Port, USB_ENUM_Pin, state)
#endif

// Miscellaneous Parameters
#define LED_TEST_DURATION   5000

#endif
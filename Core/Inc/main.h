#ifndef __MAIN_H
#define __MAIN_H

#include <stdint.h>
#include "stm32f1xx.h"
#include "stm32f1xx_ll_bus.h"
#include "stm32f1xx_ll_gpio.h"
#include "stm32f1xx_ll_rcc.h"
#include "stm32f1xx_ll_utils.h"

#define led_Pin       LL_GPIO_PIN_13
#define led_GPIO_Port GPIOC

void Error_Handler(void);

#ifndef NVIC_PRIORITYGROUP_0
#define NVIC_PRIORITYGROUP_0 ((uint32_t)0x00000007)
#define NVIC_PRIORITYGROUP_1 ((uint32_t)0x00000006)
#define NVIC_PRIORITYGROUP_2 ((uint32_t)0x00000005)
#define NVIC_PRIORITYGROUP_3 ((uint32_t)0x00000004)
#define NVIC_PRIORITYGROUP_4 ((uint32_t)0x00000003)
#endif

#endif /* __MAIN_H */

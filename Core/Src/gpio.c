#include "gpio.h"

/* PC13 is the active-low user LED on the common STM32F103C8T6 board. */
void MX_GPIO_Init(void)
{
    LL_GPIO_InitTypeDef gpio = {0};

    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_GPIOC);
    LL_GPIO_SetOutputPin(led_GPIO_Port, led_Pin); /* LED off */

    gpio.Pin = led_Pin;
    gpio.Mode = LL_GPIO_MODE_OUTPUT;
    gpio.Speed = LL_GPIO_SPEED_FREQ_LOW;
    gpio.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    gpio.Pull = LL_GPIO_PULL_UP;
    LL_GPIO_Init(led_GPIO_Port, &gpio);
}

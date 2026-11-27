#include "main.h"
#include "gpio.h"
#include "bsp_includes.h"

static void SystemClock_Config(void)
{
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_2);
    while (LL_FLASH_GetLatency() != LL_FLASH_LATENCY_2) { }

    LL_RCC_HSE_Enable();
    while (LL_RCC_HSE_IsReady() != 1U) { }

    LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSE_DIV_1, LL_RCC_PLL_MUL_9);
    LL_RCC_PLL_Enable();
    while (LL_RCC_PLL_IsReady() != 1U) { }

    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_2);
    LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
    while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL) { }

    /* HSE 8 MHz -> SYSCLK/HCLK 72 MHz; APB1/PCLK1 = 36 MHz. */
    LL_Init1msTick(72000000U);
    LL_SetSystemCoreClock(72000000U);
}

int main(void)
{
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_AFIO);
    NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
    NVIC_SetPriority(SysTick_IRQn,
                     NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 15U, 0U));

    /* Keep SWD enabled while freeing the JTAG pins. */
    LL_GPIO_AF_Remap_SWJ_NOJTAG();

    SystemClock_Config();
    MX_GPIO_Init();
    Timebase_Init();
    App_CAN_Test_Init();

    while (1)
    {
        App_CAN_Test_Task();
    }
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) { }
}

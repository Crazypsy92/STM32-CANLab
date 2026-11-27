#include "bsp_can.h"
#include "can_bxcan.h"
#include <stddef.h>
#include "stm32f1xx_ll_bus.h"
#include "stm32f1xx_ll_gpio.h"

#define BSP_CAN_PCLK1_HZ       36000000U
#define BSP_CAN_PRESCALER      4U
#define BSP_CAN_SJW_TQ         1U
#define BSP_CAN_BS1_TQ         13U
#define BSP_CAN_BS2_TQ         4U

static can_bxcan_t s_can1;
static uint8_t s_initialized = 0U;

static can_status_t map_mode(bsp_can_mode_t mode, can_bxcan_mode_t *mapped)
{
    if (mapped == NULL)
    {
        return CAN_STATUS_ERROR_PARAM;
    }

    switch (mode)
    {
        case BSP_CAN_MODE_NORMAL:
            *mapped = CAN_BXCAN_MODE_NORMAL;
            break;
        case BSP_CAN_MODE_LOOPBACK:
            *mapped = CAN_BXCAN_MODE_LOOPBACK;
            break;
        case BSP_CAN_MODE_SILENT:
            *mapped = CAN_BXCAN_MODE_SILENT;
            break;
        case BSP_CAN_MODE_SILENT_LOOPBACK:
            *mapped = CAN_BXCAN_MODE_SILENT_LOOPBACK;
            break;
        default:
            return CAN_STATUS_ERROR_PARAM;
    }

    return CAN_STATUS_OK;
}

static void configure_can_pins(void)
{
    LL_GPIO_InitTypeDef tx = {0};
    LL_GPIO_InitTypeDef rx = {0};

    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_AFIO |
                             LL_APB2_GRP1_PERIPH_GPIOB);

    /* CAN1 partial remap 2: PB8=RX, PB9=TX.
     * Change only CAN_REMAP to preserve the SWJ/SWD configuration. */
    MODIFY_REG(AFIO->MAPR, AFIO_MAPR_CAN_REMAP, AFIO_MAPR_CAN_REMAP_REMAP2);

    LL_GPIO_SetOutputPin(GPIOB, LL_GPIO_PIN_9);

    tx.Pin = LL_GPIO_PIN_9;
    tx.Mode = LL_GPIO_MODE_ALTERNATE;
    tx.Speed = LL_GPIO_SPEED_FREQ_HIGH;
    tx.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    tx.Pull = LL_GPIO_PULL_UP;
    LL_GPIO_Init(GPIOB, &tx);

    rx.Pin = LL_GPIO_PIN_8;
    rx.Mode = LL_GPIO_MODE_FLOATING;
    LL_GPIO_Init(GPIOB, &rx);
}

can_status_t BSP_CAN_Init(void)
{
    return BSP_CAN_InitWithMode(BSP_CAN_DEFAULT_MODE);
}

can_status_t BSP_CAN_InitWithMode(bsp_can_mode_t mode)
{
    can_bxcan_config_t config;
    can_bxcan_mode_t controller_mode;
    can_status_t result;

    result = map_mode(mode, &controller_mode);
    if (result != CAN_STATUS_OK)
    {
        return result;
    }

    if (s_initialized != 0U)
    {
        (void)CAN_BxCAN_DeInit(&s_can1);
        s_initialized = 0U;
    }

    configure_can_pins();

    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_CAN1);
    LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_CAN1);
    LL_APB1_GRP1_ReleaseReset(LL_APB1_GRP1_PERIPH_CAN1);

    config.pclk1_hz = BSP_CAN_PCLK1_HZ;
    config.bitrate_hz = BSP_CAN_DEFAULT_BITRATE_HZ;
    config.prescaler = BSP_CAN_PRESCALER;
    config.sjw_tq = BSP_CAN_SJW_TQ;
    config.bs1_tq = BSP_CAN_BS1_TQ;
    config.bs2_tq = BSP_CAN_BS2_TQ;
    config.mode = controller_mode;
    config.auto_bus_off = 1U;
    config.auto_retransmit = 0U;
    config.fifo_locked = 1U;

    result = CAN_BxCAN_Init(&s_can1, CAN1, &config);
    s_initialized = (result == CAN_STATUS_OK) ? 1U : 0U;
    return result;
}

can_status_t BSP_CAN_DeInit(void)
{
    can_status_t result;

    if (s_initialized == 0U)
    {
        return CAN_STATUS_ERROR_STATE;
    }

    result = CAN_BxCAN_DeInit(&s_can1);
    if (result == CAN_STATUS_OK)
    {
        s_initialized = 0U;
    }
    return result;
}

can_status_t BSP_CAN_Transmit(const can_frame_t *frame, uint32_t timeout_ms)
{
    if (s_initialized == 0U)
    {
        return CAN_STATUS_ERROR_STATE;
    }
    return CAN_BxCAN_Transmit(&s_can1, frame, timeout_ms);
}

can_status_t BSP_CAN_Receive(can_frame_t *frame, uint32_t timeout_ms)
{
    if (s_initialized == 0U)
    {
        return CAN_STATUS_ERROR_STATE;
    }
    return CAN_BxCAN_Receive(&s_can1, frame, timeout_ms);
}

can_status_t BSP_CAN_SetFilter(const can_filter_t *filter)
{
    if (s_initialized == 0U)
    {
        return CAN_STATUS_ERROR_STATE;
    }
    return CAN_BxCAN_SetFilter(&s_can1, filter);
}

can_status_t BSP_CAN_GetErrorStatus(can_error_status_t *status)
{
    if (s_initialized == 0U)
    {
        return CAN_STATUS_ERROR_STATE;
    }
    return CAN_BxCAN_GetErrorStatus(&s_can1, status);
}

uint32_t BSP_CAN_GetBitrate(void)
{
    return BSP_CAN_DEFAULT_BITRATE_HZ;
}

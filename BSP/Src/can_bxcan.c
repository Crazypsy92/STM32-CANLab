#include "can_bxcan.h"
#include "delay.h"
#include <string.h>

#define BXCAN_FILTER_BANK0       (1UL << 0U)
#define BXCAN_IDE_BIT            (1UL << 2U)
#define BXCAN_RTR_BIT            (1UL << 1U)
#define BXCAN_TXRQ_BIT           (1UL << 0U)
#define BXCAN_TME_MASK           (CAN_TSR_TME0 | CAN_TSR_TME1 | CAN_TSR_TME2)

static can_status_t wait_register_bit(volatile uint32_t *reg,
                                      uint32_t mask,
                                      uint8_t expected_set,
                                      uint32_t timeout_ms)
{
    const uint32_t start = GetTick_ms();

    do
    {
        const uint8_t is_set = ((*reg & mask) != 0U) ? 1U : 0U;
        if (is_set == expected_set)
        {
            return CAN_STATUS_OK;
        }
    } while ((GetTick_ms() - start) < timeout_ms);

    return CAN_STATUS_ERROR_TIMEOUT;
}

static uint32_t encode_filter_id(uint32_t id, can_id_type_t type)
{
    if (type == CAN_ID_STANDARD)
    {
        return (id & 0x7FFU) << 21U;
    }

    if (type == CAN_ID_EXTENDED)
    {
        return ((id & 0x1FFFFFFFU) << 3U) | BXCAN_IDE_BIT;
    }

    return 0U;
}

static can_status_t set_filter_internal(CAN_TypeDef *can,
                                        const can_filter_t *filter)
{
    uint32_t id_word;
    uint32_t mask_word;

    if (can == NULL || filter == NULL)
    {
        return CAN_STATUS_ERROR_PARAM;
    }

    if (filter->id_type == CAN_ID_STANDARD)
    {
        if (filter->id > 0x7FFU || filter->mask > 0x7FFU)
        {
            return CAN_STATUS_ERROR_PARAM;
        }
        id_word = encode_filter_id(filter->id, CAN_ID_STANDARD);
        mask_word = encode_filter_id(filter->mask, CAN_ID_STANDARD) | BXCAN_IDE_BIT;
    }
    else if (filter->id_type == CAN_ID_EXTENDED)
    {
        if (filter->id > 0x1FFFFFFFU || filter->mask > 0x1FFFFFFFU)
        {
            return CAN_STATUS_ERROR_PARAM;
        }
        id_word = encode_filter_id(filter->id, CAN_ID_EXTENDED);
        mask_word = encode_filter_id(filter->mask, CAN_ID_EXTENDED);
    }
    else if (filter->id_type == CAN_ID_ANY)
    {
        if (filter->id != 0U || filter->mask != 0U)
        {
            return CAN_STATUS_ERROR_PARAM;
        }
        id_word = 0U;
        mask_word = 0U;
    }
    else
    {
        return CAN_STATUS_ERROR_PARAM;
    }

    /* Filter bank 0: 32-bit identifier mask mode, assigned to FIFO0. */
    can->FMR |= CAN_FMR_FINIT;
    can->FA1R &= ~BXCAN_FILTER_BANK0;
    can->FM1R &= ~BXCAN_FILTER_BANK0;
    can->FS1R |= BXCAN_FILTER_BANK0;
    can->FFA1R &= ~BXCAN_FILTER_BANK0;
    can->sFilterRegister[0].FR1 = id_word;
    can->sFilterRegister[0].FR2 = mask_word;
    can->FA1R |= BXCAN_FILTER_BANK0;
    can->FMR &= ~CAN_FMR_FINIT;

    return CAN_STATUS_OK;
}

static uint8_t validate_frame(const can_frame_t *frame)
{
    if (frame == NULL || frame->dlc > CAN_FRAME_MAX_DATA_LEN)
    {
        return 0U;
    }

    if (frame->frame_type != CAN_FRAME_DATA &&
        frame->frame_type != CAN_FRAME_REMOTE)
    {
        return 0U;
    }

    if (frame->id_type == CAN_ID_STANDARD)
    {
        return (frame->id <= 0x7FFU) ? 1U : 0U;
    }

    if (frame->id_type == CAN_ID_EXTENDED)
    {
        return (frame->id <= 0x1FFFFFFFU) ? 1U : 0U;
    }

    return 0U;
}

can_status_t CAN_BxCAN_Init(can_bxcan_t *driver,
                            CAN_TypeDef *instance,
                            const can_bxcan_config_t *config)
{
    uint32_t tq_total;
    uint32_t divisor;
    uint32_t btr = 0U;
    can_filter_t accept_all = {0U, 0U, CAN_ID_ANY};

    if (driver == NULL || instance == NULL || config == NULL)
    {
        return CAN_STATUS_ERROR_PARAM;
    }

    if (config->pclk1_hz == 0U || config->bitrate_hz == 0U ||
        config->prescaler < 1U || config->prescaler > 1024U ||
        config->sjw_tq < 1U || config->sjw_tq > 4U ||
        config->bs1_tq < 1U || config->bs1_tq > 16U ||
        config->bs2_tq < 1U || config->bs2_tq > 8U ||
        config->sjw_tq > config->bs2_tq ||
        config->mode > CAN_BXCAN_MODE_SILENT_LOOPBACK)
    {
        return CAN_STATUS_ERROR_PARAM;
    }

    tq_total = 1U + (uint32_t)config->bs1_tq + (uint32_t)config->bs2_tq;
    divisor = (uint32_t)config->prescaler * tq_total;
    if (divisor == 0U || (config->pclk1_hz % divisor) != 0U ||
        (config->pclk1_hz / divisor) != config->bitrate_hz)
    {
        return CAN_STATUS_ERROR_PARAM;
    }

    driver->instance = instance;
    driver->initialized = 0U;

    instance->MCR |= CAN_MCR_INRQ;
    if (wait_register_bit(&instance->MSR, CAN_MSR_INAK, 1U, 10U) != CAN_STATUS_OK)
    {
        return CAN_STATUS_ERROR_TIMEOUT;
    }

    instance->IER = 0U;
    instance->MCR &= ~(CAN_MCR_SLEEP | CAN_MCR_AWUM | CAN_MCR_ABOM |
                       CAN_MCR_RFLM | CAN_MCR_NART | CAN_MCR_TXFP);

    if (config->auto_bus_off != 0U)
    {
        instance->MCR |= CAN_MCR_ABOM;
    }
    if (config->auto_retransmit == 0U)
    {
        instance->MCR |= CAN_MCR_NART;
    }
    if (config->fifo_locked != 0U)
    {
        instance->MCR |= CAN_MCR_RFLM;
    }

    btr = ((uint32_t)(config->prescaler - 1U) & 0x3FFU) |
          ((uint32_t)(config->bs1_tq - 1U) << 16U) |
          ((uint32_t)(config->bs2_tq - 1U) << 20U) |
          ((uint32_t)(config->sjw_tq - 1U) << 24U);

    if (config->mode == CAN_BXCAN_MODE_LOOPBACK ||
        config->mode == CAN_BXCAN_MODE_SILENT_LOOPBACK)
    {
        btr |= CAN_BTR_LBKM;
    }
    if (config->mode == CAN_BXCAN_MODE_SILENT ||
        config->mode == CAN_BXCAN_MODE_SILENT_LOOPBACK)
    {
        btr |= CAN_BTR_SILM;
    }

    instance->BTR = btr;

    if (set_filter_internal(instance, &accept_all) != CAN_STATUS_OK)
    {
        return CAN_STATUS_ERROR_STATE;
    }

    instance->MCR &= ~CAN_MCR_INRQ;
    if (wait_register_bit(&instance->MSR, CAN_MSR_INAK, 0U, 10U) != CAN_STATUS_OK)
    {
        return CAN_STATUS_ERROR_TIMEOUT;
    }

    driver->initialized = 1U;
    return CAN_STATUS_OK;
}

can_status_t CAN_BxCAN_DeInit(can_bxcan_t *driver)
{
    if (driver == NULL || driver->instance == NULL || driver->initialized == 0U)
    {
        return CAN_STATUS_ERROR_STATE;
    }

    driver->instance->IER = 0U;
    driver->instance->MCR |= CAN_MCR_INRQ;

    if (wait_register_bit(&driver->instance->MSR, CAN_MSR_INAK, 1U, 10U) != CAN_STATUS_OK)
    {
        return CAN_STATUS_ERROR_TIMEOUT;
    }

    driver->initialized = 0U;
    return CAN_STATUS_OK;
}

can_status_t CAN_BxCAN_SetFilter(can_bxcan_t *driver,
                                 const can_filter_t *filter)
{
    if (driver == NULL || driver->instance == NULL || driver->initialized == 0U)
    {
        return CAN_STATUS_ERROR_STATE;
    }

    return set_filter_internal(driver->instance, filter);
}

can_status_t CAN_BxCAN_Transmit(can_bxcan_t *driver,
                                const can_frame_t *frame,
                                uint32_t timeout_ms)
{
    CAN_TypeDef *can;
    CAN_TxMailBox_TypeDef *mailbox;
    uint32_t start;
    uint32_t tsr;
    uint32_t tir;
    uint32_t low = 0U;
    uint32_t high = 0U;
    uint32_t rqcp_mask;
    uint32_t txok_mask;
    uint32_t alst_mask;
    uint32_t terr_mask;
    uint32_t abrq_mask;
    uint8_t mailbox_index;
    uint8_t i;

    if (driver == NULL || driver->instance == NULL || driver->initialized == 0U)
    {
        return CAN_STATUS_ERROR_STATE;
    }
    if (validate_frame(frame) == 0U || timeout_ms == 0U)
    {
        return CAN_STATUS_ERROR_PARAM;
    }

    can = driver->instance;
    start = GetTick_ms();

    while ((can->TSR & BXCAN_TME_MASK) == 0U)
    {
        if ((GetTick_ms() - start) >= timeout_ms)
        {
            return CAN_STATUS_ERROR_BUSY;
        }
    }

    tsr = can->TSR;
    if ((tsr & CAN_TSR_TME0) != 0U)
    {
        mailbox_index = 0U;
    }
    else if ((tsr & CAN_TSR_TME1) != 0U)
    {
        mailbox_index = 1U;
    }
    else
    {
        mailbox_index = 2U;
    }

    rqcp_mask = 1UL << ((uint32_t)mailbox_index * 8U);
    txok_mask = 1UL << (((uint32_t)mailbox_index * 8U) + 1U);
    alst_mask = 1UL << (((uint32_t)mailbox_index * 8U) + 2U);
    terr_mask = 1UL << (((uint32_t)mailbox_index * 8U) + 3U);
    abrq_mask = 1UL << (((uint32_t)mailbox_index * 8U) + 7U);

    can->TSR = rqcp_mask; /* RQCPx is write-one-to-clear. */

    mailbox = &can->sTxMailBox[mailbox_index];
    mailbox->TDTR = (uint32_t)frame->dlc;

    if (frame->frame_type == CAN_FRAME_DATA)
    {
        for (i = 0U; i < frame->dlc; ++i)
        {
            if (i < 4U)
            {
                low |= (uint32_t)frame->data[i] << ((uint32_t)i * 8U);
            }
            else
            {
                high |= (uint32_t)frame->data[i] << (((uint32_t)i - 4U) * 8U);
            }
        }
    }

    mailbox->TDLR = low;
    mailbox->TDHR = high;

    if (frame->id_type == CAN_ID_STANDARD)
    {
        tir = (frame->id & 0x7FFU) << 21U;
    }
    else
    {
        tir = ((frame->id & 0x1FFFFFFFU) << 3U) | BXCAN_IDE_BIT;
    }

    if (frame->frame_type == CAN_FRAME_REMOTE)
    {
        tir |= BXCAN_RTR_BIT;
    }

    mailbox->TIR = tir | BXCAN_TXRQ_BIT;

    while ((can->TSR & rqcp_mask) == 0U)
    {
        if ((GetTick_ms() - start) >= timeout_ms)
        {
            can->TSR = abrq_mask;
            return CAN_STATUS_ERROR_TIMEOUT;
        }
    }

    tsr = can->TSR;
    can->TSR = rqcp_mask;

    if ((tsr & txok_mask) != 0U)
    {
        return CAN_STATUS_OK;
    }
    if ((tsr & alst_mask) != 0U)
    {
        return CAN_STATUS_ERROR_ARBITRATION;
    }
    if ((tsr & terr_mask) != 0U)
    {
        return CAN_STATUS_ERROR_TX;
    }

    return CAN_STATUS_ERROR_TX;
}

can_status_t CAN_BxCAN_Receive(can_bxcan_t *driver,
                               can_frame_t *frame,
                               uint32_t timeout_ms)
{
    CAN_TypeDef *can;
    CAN_FIFOMailBox_TypeDef *mailbox;
    uint32_t start;
    uint32_t rir;
    uint32_t rdtr;
    uint32_t low;
    uint32_t high;
    uint8_t i;

    if (driver == NULL || driver->instance == NULL || driver->initialized == 0U)
    {
        return CAN_STATUS_ERROR_STATE;
    }
    if (frame == NULL)
    {
        return CAN_STATUS_ERROR_PARAM;
    }

    can = driver->instance;
    start = GetTick_ms();

    while ((can->RF0R & CAN_RF0R_FMP0) == 0U)
    {
        if (timeout_ms == 0U || (GetTick_ms() - start) >= timeout_ms)
        {
            return CAN_STATUS_NO_MESSAGE;
        }
    }

    mailbox = &can->sFIFOMailBox[0];
    rir = mailbox->RIR;
    rdtr = mailbox->RDTR;
    low = mailbox->RDLR;
    high = mailbox->RDHR;

    memset(frame, 0, sizeof(*frame));

    if ((rir & BXCAN_IDE_BIT) != 0U)
    {
        frame->id_type = CAN_ID_EXTENDED;
        frame->id = (rir >> 3U) & 0x1FFFFFFFU;
    }
    else
    {
        frame->id_type = CAN_ID_STANDARD;
        frame->id = (rir >> 21U) & 0x7FFU;
    }

    frame->frame_type = ((rir & BXCAN_RTR_BIT) != 0U) ?
                        CAN_FRAME_REMOTE : CAN_FRAME_DATA;

    if ((rdtr & 0x0FU) > CAN_FRAME_MAX_DATA_LEN)
    {
        can->RF0R = CAN_RF0R_RFOM0;
        return CAN_STATUS_ERROR_PROTOCOL;
    }

    frame->dlc = (uint8_t)(rdtr & 0x0FU);

    if (frame->frame_type == CAN_FRAME_DATA)
    {
        for (i = 0U; i < frame->dlc; ++i)
        {
            if (i < 4U)
            {
                frame->data[i] = (uint8_t)(low >> ((uint32_t)i * 8U));
            }
            else
            {
                frame->data[i] = (uint8_t)(high >> (((uint32_t)i - 4U) * 8U));
            }
        }
    }

    can->RF0R = CAN_RF0R_RFOM0;
    return CAN_STATUS_OK;
}

can_status_t CAN_BxCAN_GetErrorStatus(can_bxcan_t *driver,
                                      can_error_status_t *status)
{
    uint32_t esr;
    uint32_t rf0r;

    if (driver == NULL || driver->instance == NULL || driver->initialized == 0U)
    {
        return CAN_STATUS_ERROR_STATE;
    }
    if (status == NULL)
    {
        return CAN_STATUS_ERROR_PARAM;
    }

    esr = driver->instance->ESR;
    rf0r = driver->instance->RF0R;

    status->tec = (uint16_t)((esr & CAN_ESR_TEC) >> 16U);
    status->rec = (uint16_t)((esr & CAN_ESR_REC) >> 24U);
    status->last_error_code = (uint8_t)((esr >> 4U) & 0x07U);
    status->error_warning = ((esr & CAN_ESR_EWGF) != 0U) ? 1U : 0U;
    status->error_passive = ((esr & CAN_ESR_EPVF) != 0U) ? 1U : 0U;
    status->bus_off = ((esr & CAN_ESR_BOFF) != 0U) ? 1U : 0U;
    status->fifo0_overrun = ((rf0r & CAN_RF0R_FOVR0) != 0U) ? 1U : 0U;

    if (status->fifo0_overrun != 0U)
    {
        driver->instance->RF0R = CAN_RF0R_FOVR0;
    }

    return CAN_STATUS_OK;
}

/**
 * @file    can_bxcan.c
 * @brief   STM32 bxCAN 寄存器级驱动实现。
 * @details 寄存器字段使用 CMSIS 设备头文件提供的位掩码和位偏移定义。
 */
#include "can_bxcan.h"
#include "delay.h"
#include <stddef.h>
#include <string.h>

#define BXCAN_FILTER_BANK0       CAN_FA1R_FACT0

/**
 * @brief bxCAN 三个发送邮箱的状态掩码。
 * @details 使用 CMSIS 定义的位掩码，避免手工计算 TSR 位偏移。
 */
typedef struct
{
    uint32_t tme;
    uint32_t rqcp;
    uint32_t txok;
    uint32_t alst;
    uint32_t terr;
    uint32_t abrq;
} can_bxcan_tx_flags_t;

static const can_bxcan_tx_flags_t s_tx_flags[3] =
{
    {CAN_TSR_TME0, CAN_TSR_RQCP0, CAN_TSR_TXOK0, CAN_TSR_ALST0, CAN_TSR_TERR0, CAN_TSR_ABRQ0},
    {CAN_TSR_TME1, CAN_TSR_RQCP1, CAN_TSR_TXOK1, CAN_TSR_ALST1, CAN_TSR_TERR1, CAN_TSR_ABRQ1},
    {CAN_TSR_TME2, CAN_TSR_RQCP2, CAN_TSR_TXOK2, CAN_TSR_ALST2, CAN_TSR_TERR2, CAN_TSR_ABRQ2}
};

static uint8_t driver_is_ready(const can_bxcan_t *driver)
{
    return (driver != NULL && driver->instance != NULL &&
            driver->state == CAN_BXCAN_STATE_READY) ? 1U : 0U;
}

/** @brief 在超时时间内等待寄存器掩码达到指定状态。 */
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

/**
 * @brief 将 CAN ID 编码到 bxCAN 32 位过滤器寄存器格式。
 * @param[in] id CAN 标识符。
 * @param[in] type 标准或扩展 ID 格式。
 * @return 已按 bxCAN ID 字段位置对齐的寄存器值。
 */
static uint32_t encode_filter_id(uint32_t id, can_id_type_t type)
{
    if (type == CAN_ID_STANDARD)
    {
        return (id & CAN_STD_ID_MASK) << CAN_RI0R_STID_Pos;
    }

    if (type == CAN_ID_EXTENDED)
    {
        return ((id & CAN_EXT_ID_MASK) << CAN_RI0R_EXID_Pos) | CAN_RI0R_IDE;
    }

    return 0U;
}

/** @brief 配置 Filter Bank 0 的 32 位 ID 掩码过滤器。 */
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
        if (filter->id > CAN_STD_ID_MASK || filter->id_mask > CAN_STD_ID_MASK)
        {
            return CAN_STATUS_ERROR_PARAM;
        }
        id_word = encode_filter_id(filter->id, CAN_ID_STANDARD);
        mask_word = encode_filter_id(filter->id_mask, CAN_ID_STANDARD) | CAN_RI0R_IDE;
    }
    else if (filter->id_type == CAN_ID_EXTENDED)
    {
        if (filter->id > CAN_EXT_ID_MASK || filter->id_mask > CAN_EXT_ID_MASK)
        {
            return CAN_STATUS_ERROR_PARAM;
        }
        id_word = encode_filter_id(filter->id, CAN_ID_EXTENDED);
        mask_word = encode_filter_id(filter->id_mask, CAN_ID_EXTENDED);
    }
    else if (filter->id_type == CAN_ID_ANY)
    {
        if (filter->id != 0U || filter->id_mask != 0U)
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
    can->FM1R &= ~CAN_FM1R_FBM0;
    can->FS1R |= CAN_FS1R_FSC0;
    can->FFA1R &= ~CAN_FFA1R_FFA0;
    can->sFilterRegister[0].FR1 = id_word;
    can->sFilterRegister[0].FR2 = mask_word;
    can->FA1R |= BXCAN_FILTER_BANK0;
    can->FMR &= ~CAN_FMR_FINIT;

    return CAN_STATUS_OK;
}

/** @brief 检查帧 ID、帧类型和 DLC 是否符合 Classical CAN 约束。 */
static uint8_t validate_frame(const can_frame_t *frame)
{
    if (frame == NULL || frame->dlc > CAN_MAX_DLC)
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
        return (frame->id <= CAN_STD_ID_MASK) ? 1U : 0U;
    }

    if (frame->id_type == CAN_ID_EXTENDED)
    {
        return (frame->id <= CAN_EXT_ID_MASK) ? 1U : 0U;
    }

    return 0U;
}

/** @brief 初始化 bxCAN 控制器、位时序和默认接收过滤器。 */
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
        config->auto_bus_off > 1U ||
        config->auto_retransmit > 1U ||
        config->fifo_locked > 1U)
    {
        return CAN_STATUS_ERROR_PARAM;
    }

    /* Validate enum values explicitly; do not rely on enum range assumptions. */
    switch (config->mode)
    {
        case CAN_BXCAN_MODE_NORMAL:
        case CAN_BXCAN_MODE_LOOPBACK:
        case CAN_BXCAN_MODE_SILENT:
        case CAN_BXCAN_MODE_SILENT_LOOPBACK:
            break;

        default:
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
    driver->state = CAN_BXCAN_STATE_INITIALIZING;

    instance->MCR |= CAN_MCR_INRQ;
    if (wait_register_bit(&instance->MSR, CAN_MSR_INAK, 1U, 10U) != CAN_STATUS_OK)
    {
        driver->state = CAN_BXCAN_STATE_FAULT;
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

    /* Encode bit timing with CMSIS field positions and masks, not magic shifts. */
    btr = (((uint32_t)(config->prescaler - 1U) << CAN_BTR_BRP_Pos) & CAN_BTR_BRP_Msk) |
          (((uint32_t)(config->bs1_tq - 1U) << CAN_BTR_TS1_Pos) & CAN_BTR_TS1_Msk) |
          (((uint32_t)(config->bs2_tq - 1U) << CAN_BTR_TS2_Pos) & CAN_BTR_TS2_Msk) |
          (((uint32_t)(config->sjw_tq - 1U) << CAN_BTR_SJW_Pos) & CAN_BTR_SJW_Msk);

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
        driver->state = CAN_BXCAN_STATE_FAULT;
        return CAN_STATUS_ERROR_STATE;
    }

    instance->MCR &= ~CAN_MCR_INRQ;
    if (wait_register_bit(&instance->MSR, CAN_MSR_INAK, 0U, 10U) != CAN_STATUS_OK)
    {
        driver->state = CAN_BXCAN_STATE_FAULT;
        return CAN_STATUS_ERROR_TIMEOUT;
    }

    driver->state = CAN_BXCAN_STATE_READY;
    return CAN_STATUS_OK;
}

/** @brief 停止 bxCAN 控制器并请求进入初始化模式。 */
can_status_t CAN_BxCAN_Stop(can_bxcan_t *driver)
{
    if (driver_is_ready(driver) == 0U)
    {
        return CAN_STATUS_ERROR_STATE;
    }

    driver->state = CAN_BXCAN_STATE_STOPPING;
    driver->instance->IER = 0U;
    driver->instance->MCR |= CAN_MCR_INRQ;

    if (wait_register_bit(&driver->instance->MSR, CAN_MSR_INAK, 1U, 10U) != CAN_STATUS_OK)
    {
        driver->state = CAN_BXCAN_STATE_FAULT;
        return CAN_STATUS_ERROR_TIMEOUT;
    }

    driver->state = CAN_BXCAN_STATE_STOPPED;
    return CAN_STATUS_OK;
}

/** @brief 更新 bxCAN Filter Bank 0 的接收条件。 */
can_status_t CAN_BxCAN_SetFilter(can_bxcan_t *driver,
                                 const can_filter_t *filter)
{
    if (driver_is_ready(driver) == 0U)
    {
        return CAN_STATUS_ERROR_STATE;
    }

    return set_filter_internal(driver->instance, filter);
}

/** @brief 轮询可用发送邮箱并等待本次发送请求完成。 */
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
    const can_bxcan_tx_flags_t *tx_flags;
    uint8_t mailbox_index;
    uint8_t i;

    if (driver_is_ready(driver) == 0U)
    {
        return CAN_STATUS_ERROR_STATE;
    }
    if (validate_frame(frame) == 0U || timeout_ms == 0U)
    {
        return CAN_STATUS_ERROR_PARAM;
    }

    can = driver->instance;
    start = GetTick_ms();

    while ((can->TSR & CAN_TSR_TME) == 0U)
    {
        if ((GetTick_ms() - start) >= timeout_ms)
        {
            return CAN_STATUS_ERROR_BUSY;
        }
    }

    tsr = can->TSR;
    if ((tsr & s_tx_flags[0].tme) != 0U)
    {
        mailbox_index = 0U;
    }
    else if ((tsr & s_tx_flags[1].tme) != 0U)
    {
        mailbox_index = 1U;
    }
    else
    {
        mailbox_index = 2U;
    }

    tx_flags = &s_tx_flags[mailbox_index];

    /* RQCPx is write-one-to-clear; clear stale completion before reusing mailbox. */
    can->TSR = tx_flags->rqcp;

    mailbox = &can->sTxMailBox[mailbox_index];
    mailbox->TDTR = (uint32_t)frame->dlc & CAN_TDT0R_DLC;

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
        tir = (frame->id & CAN_STD_ID_MASK) << CAN_TI0R_STID_Pos;
    }
    else
    {
        tir = ((frame->id & CAN_EXT_ID_MASK) << CAN_TI0R_EXID_Pos) | CAN_TI0R_IDE;
    }

    if (frame->frame_type == CAN_FRAME_REMOTE)
    {
        tir |= CAN_TI0R_RTR;
    }

    mailbox->TIR = tir | CAN_TI0R_TXRQ;

    while ((can->TSR & tx_flags->rqcp) == 0U)
    {
        if ((GetTick_ms() - start) >= timeout_ms)
        {
            can->TSR = tx_flags->abrq;
            return CAN_STATUS_ERROR_TIMEOUT;
        }
    }

    tsr = can->TSR;
    can->TSR = tx_flags->rqcp;

    if ((tsr & tx_flags->txok) != 0U)
    {
        return CAN_STATUS_OK;
    }
    if ((tsr & tx_flags->alst) != 0U)
    {
        return CAN_STATUS_ERROR_ARBITRATION;
    }
    if ((tsr & tx_flags->terr) != 0U)
    {
        return CAN_STATUS_ERROR_TX;
    }

    return CAN_STATUS_ERROR_TX;
}

/** @brief 从 FIFO0 读取一帧并解码为通用 CAN 帧结构。 */
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

    if (driver_is_ready(driver) == 0U)
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
        if (timeout_ms == 0U)
        {
            return CAN_STATUS_NO_MESSAGE;
        }

        if ((GetTick_ms() - start) >= timeout_ms)
        {
            return CAN_STATUS_ERROR_TIMEOUT;
        }
    }

    mailbox = &can->sFIFOMailBox[0];
    rir = mailbox->RIR;
    rdtr = mailbox->RDTR;
    low = mailbox->RDLR;
    high = mailbox->RDHR;

    memset(frame, 0, sizeof(*frame));

    if ((rir & CAN_RI0R_IDE) != 0U)
    {
        frame->id_type = CAN_ID_EXTENDED;
        frame->id = (rir >> CAN_RI0R_EXID_Pos) & CAN_EXT_ID_MASK;
    }
    else
    {
        frame->id_type = CAN_ID_STANDARD;
        frame->id = (rir >> CAN_RI0R_STID_Pos) & CAN_STD_ID_MASK;
    }

    frame->frame_type = ((rir & CAN_RI0R_RTR) != 0U) ?
                        CAN_FRAME_REMOTE : CAN_FRAME_DATA;

    if ((rdtr & CAN_RDT0R_DLC) > CAN_MAX_DLC)
    {
        can->RF0R = CAN_RF0R_RFOM0;
        return CAN_STATUS_ERROR_PROTOCOL;
    }

    frame->dlc = (uint8_t)(rdtr & CAN_RDT0R_DLC);

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

/**
 * @brief 将 bxCAN LEC 寄存器编码转换为通用 CAN 错误类别。
 * @param[in] lec bxCAN ESR.LEC 字段。
 * @return can_error_code_t 通用错误类别。
 */
static can_error_code_t decode_last_error(uint32_t lec)
{
    switch (lec)
    {
        case 0U:
            return CAN_ERROR_CODE_NONE;
        case 1U:
            return CAN_ERROR_CODE_STUFF;
        case 2U:
            return CAN_ERROR_CODE_FORM;
        case 3U:
            return CAN_ERROR_CODE_ACK;
        case 4U:
            return CAN_ERROR_CODE_BIT_RECESSIVE;
        case 5U:
            return CAN_ERROR_CODE_BIT_DOMINANT;
        case 6U:
            return CAN_ERROR_CODE_CRC;
        default:
            return CAN_ERROR_CODE_NOT_REPORTED;
    }
}

/** @brief 读取 TEC、REC、LEC、错误状态和 FIFO0 溢出标志。 */
can_status_t CAN_BxCAN_GetErrorStatus(can_bxcan_t *driver,
                                      can_error_status_t *status)
{
    uint32_t esr;
    uint32_t rf0r;

    if (driver_is_ready(driver) == 0U)
    {
        return CAN_STATUS_ERROR_STATE;
    }
    if (status == NULL)
    {
        return CAN_STATUS_ERROR_PARAM;
    }

    esr = driver->instance->ESR;
    rf0r = driver->instance->RF0R;

    status->tec = (uint16_t)((esr & CAN_ESR_TEC) >> CAN_ESR_TEC_Pos);
    status->rec = (uint16_t)((esr & CAN_ESR_REC) >> CAN_ESR_REC_Pos);
    status->last_error_code = decode_last_error((esr & CAN_ESR_LEC) >> CAN_ESR_LEC_Pos);
    status->error_warning = ((esr & CAN_ESR_EWGF) != 0U) ? 1U : 0U;
    status->error_passive = ((esr & CAN_ESR_EPVF) != 0U) ? 1U : 0U;
    status->bus_off = ((esr & CAN_ESR_BOFF) != 0U) ? 1U : 0U;
    status->fifo0_overrun = ((rf0r & CAN_RF0R_FOVR0) != 0U) ? 1U : 0U;

    return CAN_STATUS_OK;
}

/**
 * @brief 清除 bxCAN FIFO0 溢出标志（写 1 清除）。
 * @param[in] driver 已初始化的驱动句柄。
 * @return can_status_t 操作结果。
 */
can_status_t CAN_BxCAN_ClearFifo0Overrun(can_bxcan_t *driver)
{
    if (driver_is_ready(driver) == 0U)
    {
        return CAN_STATUS_ERROR_STATE;
    }

    driver->instance->RF0R = CAN_RF0R_FOVR0;
    return CAN_STATUS_OK;
}

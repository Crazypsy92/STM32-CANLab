#ifndef __CAN_BXCAN_H
#define __CAN_BXCAN_H

#include <stdint.h>
#include "stm32f1xx.h"
#include "can_frame.h"

typedef enum
{
    CAN_BXCAN_MODE_NORMAL = 0,
    CAN_BXCAN_MODE_LOOPBACK,
    CAN_BXCAN_MODE_SILENT,
    CAN_BXCAN_MODE_SILENT_LOOPBACK
} can_bxcan_mode_t;

typedef struct
{
    uint32_t pclk1_hz;
    uint32_t bitrate_hz;
    uint16_t prescaler;
    uint8_t sjw_tq;
    uint8_t bs1_tq;
    uint8_t bs2_tq;
    can_bxcan_mode_t mode;
    uint8_t auto_bus_off;
    uint8_t auto_retransmit;
    uint8_t fifo_locked;
} can_bxcan_config_t;

typedef struct
{
    CAN_TypeDef *instance;
    uint8_t initialized;
} can_bxcan_t;

can_status_t CAN_BxCAN_Init(can_bxcan_t *driver,
                            CAN_TypeDef *instance,
                            const can_bxcan_config_t *config);
can_status_t CAN_BxCAN_DeInit(can_bxcan_t *driver);
can_status_t CAN_BxCAN_SetFilter(can_bxcan_t *driver,
                                 const can_filter_t *filter);
can_status_t CAN_BxCAN_Transmit(can_bxcan_t *driver,
                                const can_frame_t *frame,
                                uint32_t timeout_ms);
can_status_t CAN_BxCAN_Receive(can_bxcan_t *driver,
                               can_frame_t *frame,
                               uint32_t timeout_ms);
can_status_t CAN_BxCAN_GetErrorStatus(can_bxcan_t *driver,
                                      can_error_status_t *status);

#endif /* __CAN_BXCAN_H */

#ifndef __BSP_CAN_H
#define __BSP_CAN_H

#include <stdint.h>
#include "can_frame.h"

typedef enum
{
    BSP_CAN_MODE_NORMAL = 0,
    BSP_CAN_MODE_LOOPBACK,
    BSP_CAN_MODE_SILENT,
    BSP_CAN_MODE_SILENT_LOOPBACK
} bsp_can_mode_t;

/* Silent internal loopback is the initial default; it does not require a transceiver and keeps CAN_TX recessive. */
#ifndef BSP_CAN_DEFAULT_MODE
#define BSP_CAN_DEFAULT_MODE BSP_CAN_MODE_SILENT_LOOPBACK
#endif

#define BSP_CAN_DEFAULT_BITRATE_HZ 500000U

can_status_t BSP_CAN_Init(void);
can_status_t BSP_CAN_InitWithMode(bsp_can_mode_t mode);
can_status_t BSP_CAN_DeInit(void);
can_status_t BSP_CAN_Transmit(const can_frame_t *frame, uint32_t timeout_ms);
can_status_t BSP_CAN_Receive(can_frame_t *frame, uint32_t timeout_ms);
can_status_t BSP_CAN_SetFilter(const can_filter_t *filter);
can_status_t BSP_CAN_GetErrorStatus(can_error_status_t *status);
uint32_t BSP_CAN_GetBitrate(void);

#endif /* __BSP_CAN_H */

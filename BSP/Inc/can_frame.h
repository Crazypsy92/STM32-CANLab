#ifndef __CAN_FRAME_H
#define __CAN_FRAME_H

#include <stdint.h>

#define CAN_FRAME_MAX_DATA_LEN 8U

typedef enum
{
    CAN_ID_STANDARD = 0,
    CAN_ID_EXTENDED = 1,
    CAN_ID_ANY = 2
} can_id_type_t;

typedef enum
{
    CAN_FRAME_DATA = 0,
    CAN_FRAME_REMOTE = 1
} can_frame_type_t;

typedef enum
{
    CAN_STATUS_OK = 0,
    CAN_STATUS_NO_MESSAGE = 1,
    CAN_STATUS_ERROR_PARAM = -1,
    CAN_STATUS_ERROR_TIMEOUT = -2,
    CAN_STATUS_ERROR_BUSY = -3,
    CAN_STATUS_ERROR_STATE = -4,
    CAN_STATUS_ERROR_TX = -5,
    CAN_STATUS_ERROR_ARBITRATION = -6,
    CAN_STATUS_ERROR_PROTOCOL = -7
} can_status_t;

typedef struct
{
    uint32_t id;
    can_id_type_t id_type;
    can_frame_type_t frame_type;
    uint8_t dlc;
    uint8_t data[CAN_FRAME_MAX_DATA_LEN];
} can_frame_t;

/* One 32-bit mask filter is exposed initially; IDE is matched automatically
 * for STANDARD or EXTENDED. CAN_ID_ANY accepts both formats. */
typedef struct
{
    uint32_t id;
    uint32_t mask;
    can_id_type_t id_type;
} can_filter_t;

typedef struct
{
    uint16_t tec;
    uint16_t rec;
    uint8_t last_error_code;
    uint8_t error_warning;
    uint8_t error_passive;
    uint8_t bus_off;
    uint8_t fifo0_overrun;
} can_error_status_t;

#endif /* __CAN_FRAME_H */

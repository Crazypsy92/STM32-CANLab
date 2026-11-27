/**
 * @file    can_frame.h
 * @brief   CAN 2.0 通用数据类型定义。
 * @details 本文件不依赖具体 MCU，可供应用层和不同 CAN 控制器后端共同使用。
 */
#ifndef CAN_FRAME_H
#define CAN_FRAME_H

#include <stdint.h>

/** @brief 标准 CAN 标识符有效位掩码（11 bit）。 */
#define CAN_STD_ID_MASK       (0x7FFUL)
/** @brief 扩展 CAN 标识符有效位掩码（29 bit）。 */
#define CAN_EXT_ID_MASK       (0x1FFFFFFFUL)
/** @brief Classical CAN 单帧最大数据长度（字节）。 */
#define CAN_MAX_DLC           (8U)

/** @brief CAN 标识符格式。 */
typedef uint8_t can_id_type_t;
enum
{
    CAN_ID_STANDARD = 0U, /**< 标准帧标识符，11 bit。 */
    CAN_ID_EXTENDED = 1U, /**< 扩展帧标识符，29 bit。 */
    CAN_ID_ANY = 2U        /**< 仅用于接收过滤器，表示不限制 IDE 格式。 */
};

/** @brief CAN 帧类型。 */
typedef uint8_t can_frame_type_t;
enum
{
    CAN_FRAME_DATA = 0U,   /**< 数据帧，data[] 携带有效载荷。 */
    CAN_FRAME_REMOTE = 1U  /**< 远程帧，不携带数据，DLC 表示请求的数据长度。 */

};

/**
 * @brief CAN API 返回状态。
 * @note CAN_STATUS_NO_MESSAGE 是接收接口的正常状态，不代表硬件故障。
 */
typedef enum
{
    CAN_STATUS_OK = 0,                 /**< 操作成功。 */
    CAN_STATUS_NO_MESSAGE = 1,         /**< 非阻塞接收时 FIFO 当前为空。 */
    CAN_STATUS_ERROR_PARAM = -1,       /**< 参数非法或超出协议范围。 */
    CAN_STATUS_ERROR_TIMEOUT = -2,     /**< 等待操作超时。 */
    CAN_STATUS_ERROR_BUSY = -3,        /**< 发送邮箱在指定时间内未空闲。 */
    CAN_STATUS_ERROR_STATE = -4,       /**< 控制器未初始化或状态不允许该操作。 */
    CAN_STATUS_ERROR_TX = -5,          /**< 发送失败，且不是单纯的仲裁丢失。 */
    CAN_STATUS_ERROR_ARBITRATION = -6, /**< 本次发送请求报告仲裁丢失。 */
    CAN_STATUS_ERROR_PROTOCOL = -7     /**< 接收到的控制器数据不符合预期格式。 */
} can_status_t;

/** @brief CAN 控制器报告的通用错误类别，与具体控制器寄存器编码无关。 */
typedef uint8_t can_error_code_t;
enum
{
    CAN_ERROR_CODE_NONE = 0U,          /**< 无错误。 */
    CAN_ERROR_CODE_STUFF = 1U,         /**< 位填充错误。 */
    CAN_ERROR_CODE_FORM = 2U,          /**< 帧格式错误。 */
    CAN_ERROR_CODE_ACK = 3U,           /**< ACK 错误。 */
    CAN_ERROR_CODE_BIT_RECESSIVE = 4U, /**< 发送隐性位时检测到显性位。 */
    CAN_ERROR_CODE_BIT_DOMINANT = 5U,  /**< 发送显性位时检测到隐性位。 */
    CAN_ERROR_CODE_CRC = 6U,           /**< CRC 错误。 */
    CAN_ERROR_CODE_NOT_REPORTED = 7U /**< 控制器未提供新的错误代码。 */

};

/** @brief Classical CAN 帧。 */
typedef struct
{
    uint32_t id;                 /**< CAN ID；范围由 id_type 决定。 */
    can_id_type_t id_type;       /**< 标准 11 bit 或扩展 29 bit。 */
    can_frame_type_t frame_type; /**< 数据帧或远程帧。 */
    uint8_t dlc;                 /**< 数据长度代码，范围 0..8；远程帧表示请求长度。 */
    uint8_t data[CAN_MAX_DLC];   /**< 数据帧有效载荷；远程帧忽略此数组。 */
} can_frame_t;

/**
 * @brief CAN 接收 ID 掩码过滤器。
 *
 * @details 对每一个 ID 位：
 *          - id_mask 对应位为 1：该位必须与 id 对应位相同；
 *          - id_mask 对应位为 0：该位不参与比较（don't care）。
 *
 *          CAN_ID_STANDARD / CAN_ID_EXTENDED 同时要求 IDE 格式匹配；
 *          CAN_ID_ANY 接收任意 ID 和 IDE 格式，此时 id 和 id_mask 必须都为 0。
 *          当前 bxCAN 后端不对 RTR 位单独过滤。
 */
typedef struct
{
    uint32_t id;             /**< 期望匹配的 CAN ID。 */
    uint32_t id_mask;        /**< ID 比较掩码；1=比较，0=忽略。 */
    can_id_type_t id_type;   /**< 匹配的 ID 格式，或 CAN_ID_ANY。 */
} can_filter_t;

/** @brief CAN 控制器错误状态快照。 */
typedef struct
{
    uint16_t tec;                     /**< 发送错误计数器（TEC）当前可读值。 */
    uint16_t rec;                     /**< 接收错误计数器（REC）。 */
    can_error_code_t last_error_code; /**< 最后错误代码（LEC）。 */
    uint8_t error_warning;            /**< 错误警告状态，1=已进入 Error Warning。 */
    uint8_t error_passive;            /**< 被动错误状态，1=已进入 Error Passive。 */
    uint8_t bus_off;                  /**< Bus-Off 状态，1=已进入 Bus-Off。 */
    uint8_t fifo0_overrun;            /**< FIFO0 溢出标志，1=检测到溢出。 */
} can_error_status_t;

#endif /* CAN_FRAME_H */

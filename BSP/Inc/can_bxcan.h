/**
 * @file    can_bxcan.h
 * @brief   STM32 bxCAN 控制器后端接口。
 * @details 本层直接操作 STM32 bxCAN 寄存器，不负责 GPIO 引脚复用和外部收发器配置。
 *          调用者必须先开启 CAN 外设时钟并完成对应引脚配置。
 */
#ifndef CAN_BXCAN_H
#define CAN_BXCAN_H

#include <stdint.h>
#include "stm32f1xx.h"
#include "can_frame.h"

/** @brief bxCAN 工作模式。 */
typedef enum
{
    CAN_BXCAN_MODE_NORMAL = 0,     /**< 正常总线通信模式。 */
    CAN_BXCAN_MODE_LOOPBACK,       /**< 内部回环模式。 */
    CAN_BXCAN_MODE_SILENT,         /**< 只听模式，不主动驱动总线。 */
    CAN_BXCAN_MODE_SILENT_LOOPBACK /**< 静默内部回环模式。 */
} can_bxcan_mode_t;

/**
 * @brief bxCAN 初始化参数。
 * @note prescaler、sjw_tq、bs1_tq、bs2_tq 使用实际 TQ 数量，不是寄存器编码值。
 *       auto_bus_off、auto_retransmit、fifo_locked 仅允许填写 0 或 1。
 */
typedef struct
{
    uint32_t pclk1_hz;       /**< CAN 外设时钟 PCLK1，单位 Hz。 */
    uint32_t bitrate_hz;     /**< 目标 CAN 波特率，单位 bit/s。 */
    uint16_t prescaler;      /**< 波特率分频系数，范围 1..1024。 */
    uint8_t sjw_tq;          /**< 同步跳转宽度，范围 1..4 TQ。 */
    uint8_t bs1_tq;          /**< 位段 1，范围 1..16 TQ。 */
    uint8_t bs2_tq;          /**< 位段 2，范围 1..8 TQ。 */
    can_bxcan_mode_t mode;   /**< 控制器工作模式。 */
    uint8_t auto_bus_off;    /**< 自动 Bus-Off 恢复管理：0=关闭，1=开启。 */
    uint8_t auto_retransmit; /**< 自动重传：0=关闭，1=开启。 */
    uint8_t fifo_locked;     /**< FIFO 锁定模式：0=关闭，1=开启。 */
} can_bxcan_config_t;

/** @brief bxCAN 驱动生命周期状态。 */
typedef uint8_t can_bxcan_state_t;
enum
{
    CAN_BXCAN_STATE_UNINITIALIZED = 0U, /**< 尚未初始化。 */
    CAN_BXCAN_STATE_INITIALIZING,       /**< 正在执行初始化或停止操作。 */
    CAN_BXCAN_STATE_READY,              /**< 初始化完成，可以收发。 */
    CAN_BXCAN_STATE_STOPPED,            /**< 已停止，允许重新初始化。 */
    CAN_BXCAN_STATE_FAULT               /**< 初始化或停止过程发生故障。 */
};

/** @brief bxCAN 驱动实例句柄。 */
typedef struct
{
    CAN_TypeDef *instance;   /**< bxCAN 外设寄存器基地址，例如 CAN1。 */
    can_bxcan_state_t state; /**< 当前驱动生命周期状态。 */
} can_bxcan_t;

/**
 * @brief 初始化 bxCAN 控制器。
 * @param[out] driver 驱动实例句柄。
 * @param[in] instance bxCAN 外设实例，例如 CAN1。
 * @param[in] config 波特率、位时序和工作模式配置。
 * @return can_status_t
 * @retval CAN_STATUS_OK 初始化成功。
 * @retval CAN_STATUS_ERROR_PARAM 参数非法或波特率计算不匹配。
 * @retval CAN_STATUS_ERROR_TIMEOUT 控制器未能在规定时间内进入/退出初始化模式。
 * @note 调用前必须开启 CAN 外设时钟、配置 CAN_TX/CAN_RX 引脚，并初始化毫秒时间基准。
 */
can_status_t CAN_BxCAN_Init(can_bxcan_t *driver,
                            CAN_TypeDef *instance,
                            const can_bxcan_config_t *config);

/**
 * @brief 停止 bxCAN 控制器并请求进入初始化模式。
 * @param[in,out] driver 已初始化的驱动句柄。
 * @return can_status_t
 * @retval CAN_STATUS_OK 控制器已进入初始化模式。
 * @retval CAN_STATUS_ERROR_STATE 驱动句柄无效或尚未初始化。
 * @retval CAN_STATUS_ERROR_TIMEOUT 控制器未能在超时时间内进入初始化模式。
 */
can_status_t CAN_BxCAN_Stop(can_bxcan_t *driver);

/**
 * @brief 配置 bxCAN Filter Bank 0 的 32 位 ID 掩码过滤器。
 * @param[in,out] driver 已初始化的驱动句柄。
 * @param[in] filter ID、ID 掩码和 ID 格式配置。
 * @return can_status_t
 * @retval CAN_STATUS_OK 过滤器配置成功。
 * @retval CAN_STATUS_ERROR_PARAM 过滤器指针为空或 ID 配置超出范围。
 * @retval CAN_STATUS_ERROR_STATE 驱动句柄无效或尚未初始化。
 * @note 当前后端只使用 Filter Bank 0，并将通过过滤的帧路由到 FIFO0。
 */
can_status_t CAN_BxCAN_SetFilter(can_bxcan_t *driver,
                                 const can_filter_t *filter);

/**
 * @brief 发送一帧 Classical CAN 报文。
 * @param[in,out] driver 已初始化的驱动句柄。
 * @param[in] frame 待发送 CAN 帧。
 * @param[in] timeout_ms 等待发送邮箱及发送完成的总超时时间，单位 ms，必须大于 0。
 * @return can_status_t 发送结果。
 * @retval CAN_STATUS_OK 报文发送成功。
 * @retval CAN_STATUS_ERROR_BUSY 超时前没有可用发送邮箱。
 * @retval CAN_STATUS_ERROR_TIMEOUT 发送完成等待超时。
 * @retval CAN_STATUS_ERROR_ARBITRATION 本次发送请求报告仲裁丢失。
 * @note 本接口采用轮询并阻塞等待发送结果；不是异步发送接口。
 *       当前驱动没有内部互斥锁，多任务或中断环境下调用者必须自行串行化发送请求。
 */
can_status_t CAN_BxCAN_Transmit(can_bxcan_t *driver,
                                const can_frame_t *frame,
                                uint32_t timeout_ms);

/**
 * @brief 从 bxCAN FIFO0 接收一帧报文。
 * @param[in,out] driver 已初始化的驱动句柄。
 * @param[out] frame 接收帧输出缓冲区。
 * @param[in] timeout_ms 等待时间；0 表示立即返回，不等待。
 * @return can_status_t 接收结果。
 * @retval CAN_STATUS_OK 成功接收到一帧。
 * @retval CAN_STATUS_NO_MESSAGE 非阻塞模式下 FIFO0 当前为空。
 * @retval CAN_STATUS_ERROR_TIMEOUT 阻塞等待超过 timeout_ms。
 * @note 当前驱动没有内部互斥锁；同一 FIFO0 不应由多个上下文并发读取。
 */
can_status_t CAN_BxCAN_Receive(can_bxcan_t *driver,
                               can_frame_t *frame,
                               uint32_t timeout_ms);

/**
 * @brief 读取 bxCAN 错误状态快照。
 * @param[in] driver 已初始化的驱动句柄。
 * @param[out] status 错误状态输出结构体。
 * @return can_status_t 操作结果。
 * @note 本函数只读取状态，不清除 FIFO0 溢出标志。
 */
can_status_t CAN_BxCAN_GetErrorStatus(can_bxcan_t *driver,
                                      can_error_status_t *status);

/**
 * @brief 清除 bxCAN FIFO0 溢出标志。
 * @param[in] driver 已初始化的驱动句柄。
 * @return can_status_t
 * @retval CAN_STATUS_OK 溢出标志已清除。
 * @retval CAN_STATUS_ERROR_STATE 驱动句柄无效或尚未初始化。
 */
can_status_t CAN_BxCAN_ClearFifo0Overrun(can_bxcan_t *driver);

#endif /* CAN_BXCAN_H */

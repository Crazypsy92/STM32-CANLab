/**
 * @file    bsp_can.h
 * @brief   当前 STM32F103C8T6 板级 CAN 接口。
 * @details 本层封装 CAN1 时钟、PB8/PB9 引脚复用和板级位时序。
 *          应用层只依赖本头文件，不直接访问 bxCAN 寄存器。
 */
#ifndef BSP_CAN_H
#define BSP_CAN_H

#include <stdint.h>
#include "can_frame.h"

/** @brief 板级 CAN 工作模式。 */
typedef enum
{
    BSP_CAN_MODE_NORMAL = 0,     /**< 真实 CAN 总线通信。 */
    BSP_CAN_MODE_LOOPBACK,       /**< 内部回环。 */
    BSP_CAN_MODE_SILENT,         /**< 静默监听总线。 */
    BSP_CAN_MODE_SILENT_LOOPBACK /**< 静默内部回环，不主动驱动 CAN_TX。 */
} bsp_can_mode_t;

/** @brief 默认 CAN 模式；初始调试阶段使用静默内部回环。 */
#ifndef BSP_CAN_DEFAULT_MODE
#define BSP_CAN_DEFAULT_MODE BSP_CAN_MODE_SILENT_LOOPBACK
#endif

/** @brief 当前板级 CAN 默认波特率，单位 bit/s。 */
#define BSP_CAN_DEFAULT_BITRATE_HZ (500000U)

/**
 * @brief 按板级默认参数初始化 CAN1。
 * @return can_status_t 操作结果。
 * @note 默认 PCLK1=36 MHz、500 kbit/s；使用 BSP_CAN_DEFAULT_MODE 指定工作模式。
 */
can_status_t BSP_CAN_Init(void);

/**
 * @brief 按指定工作模式初始化 CAN1。
 * @param[in] mode CAN 工作模式。
 * @return can_status_t 操作结果。
 * @note 当前波特率和位时序仍由板级配置固定，不通过此接口动态修改。
 */
can_status_t BSP_CAN_InitWithMode(bsp_can_mode_t mode);

/**
 * @brief 停止 CAN1 并关闭 CAN1 外设时钟。
 * @return can_status_t 操作结果。
 */
can_status_t BSP_CAN_DeInit(void);

/**
 * @brief 发送一帧 CAN 报文。
 * @param[in] frame 待发送报文。
 * @param[in] timeout_ms 发送超时时间，单位 ms，必须大于 0。
 * @return can_status_t 发送结果。
 * @note 本接口轮询等待发送完成；当前驱动没有内部互斥锁，并发发送调用必须由上层串行化。
 */
can_status_t BSP_CAN_Transmit(const can_frame_t *frame, uint32_t timeout_ms);

/**
 * @brief 从 FIFO0 接收一帧 CAN 报文。
 * @param[out] frame 接收帧输出缓冲区。
 * @param[in] timeout_ms 等待时间；0 表示非阻塞接收。
 * @return can_status_t 接收结果。
 * @note timeout_ms=0 时立即返回；非零时阻塞等待。当前 FIFO0 不支持多个上下文并发读取。
 */
can_status_t BSP_CAN_Receive(can_frame_t *frame, uint32_t timeout_ms);

/**
 * @brief 设置 CAN 接收 ID 掩码过滤器。
 * @param[in] filter ID、ID 掩码和 ID 格式。
 * @return can_status_t 操作结果。
 * @note 当前仅实现一个 32 位过滤器，匹配成功的帧进入 FIFO0。
 */
can_status_t BSP_CAN_SetFilter(const can_filter_t *filter);

/**
 * @brief 读取 CAN 控制器错误状态。
 * @param[out] status 错误状态输出结构体。
 * @return can_status_t 操作结果。
 */
can_status_t BSP_CAN_GetErrorStatus(can_error_status_t *status);

/**
 * @brief 清除 CAN FIFO0 溢出标志。
 * @return can_status_t 操作结果。
 */
can_status_t BSP_CAN_ClearFifo0Overrun(void);

/**
 * @brief 获取当前板级 CAN 波特率。
 * @return 当前波特率，单位 bit/s。
 */
uint32_t BSP_CAN_GetBitrate(void);

#endif /* BSP_CAN_H */

/**
 * @file    app_can_test.h
 * @brief   CAN 控制器回环测试应用接口。
 */
#ifndef APP_CAN_TEST_H
#define APP_CAN_TEST_H

#include <stdint.h>
#include "can_frame.h"

/**
 * @brief CAN 回环测试统计信息。
 * @note 全局统计对象声明为 volatile，便于调试器观察主循环更新的测试状态。
 */
typedef struct
{
    uint32_t initialized;              /**< BSP 初始化成功标志。 */
    int32_t init_status;                /**< BSP_CAN_Init() 返回状态。 */
    uint32_t tx_ok;                     /**< 发送成功次数。 */
    uint32_t tx_error;                  /**< 发送失败次数。 */
    uint32_t rx_ok;                     /**< 成功接收次数。 */
    uint32_t rx_match;                  /**< 收发内容匹配次数。 */
    uint32_t rx_mismatch;               /**< 收发内容不匹配次数。 */
    uint32_t last_tx_id;                /**< 最近一次发送帧 ID。 */
    uint32_t last_rx_id;                /**< 最近一次接收帧 ID。 */
    uint8_t last_tx_id_type;            /**< 最近一次发送帧 ID 格式。 */
    uint8_t last_tx_frame_type;         /**< 最近一次发送帧类型。 */
    uint8_t last_tx_dlc;                /**< 最近一次发送帧 DLC。 */
    uint8_t last_rx_id_type;            /**< 最近一次接收帧 ID 格式。 */
    uint8_t last_rx_frame_type;         /**< 最近一次接收帧类型。 */
    uint8_t last_rx_dlc;                /**< 最近一次接收帧 DLC。 */
    uint8_t last_rx_data[CAN_MAX_DLC];  /**< 最近一次接收的数据帧载荷。 */
    uint16_t tec;                       /**< 发送错误计数器。 */
    uint16_t rec;                       /**< 接收错误计数器。 */
    uint8_t error_warning;              /**< Error Warning 状态。 */
    uint8_t error_passive;               /**< Error Passive 状态。 */
    uint8_t bus_off;                    /**< Bus-Off 状态。 */
    uint8_t fifo0_overrun;               /**< FIFO0 溢出标志。 */
} app_can_test_stats_t;

/** @brief CAN 回环测试统计信息，全局调试器观察入口。 */
extern volatile app_can_test_stats_t g_can_test_stats;

/**
 * @brief 初始化 CAN 回环测试。
 * @note 初始化 BSP CAN，并清零测试统计信息。
 */
void App_CAN_Test_Init(void);

/**
 * @brief 执行一次 CAN 回环测试轮询。
 * @note 由主循环周期调用；本函数内部按 100 ms 周期发送测试帧。
 */
void App_CAN_Test_Task(void);

#endif /* APP_CAN_TEST_H */

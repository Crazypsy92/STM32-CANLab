/**
 * @file    app_can_test.c
 * @brief   Classical CAN 标准帧/扩展帧、数据帧/远程帧回环测试。
 */
#include "app_can_test.h"
#include "bsp_can.h"
#include "delay.h"
#include <string.h>

volatile app_can_test_stats_t g_can_test_stats;

static can_frame_t s_expected_frame;
static uint8_t s_expected_valid = 0U;
static uint32_t s_sequence = 0U;
static uint32_t s_last_tx_tick = 0U;

/** @brief 根据递增序号生成四种 CAN 帧组合。 */
static can_frame_t make_test_frame(uint32_t sequence)
{
    can_frame_t frame;
    uint8_t i;

    memset(&frame, 0, sizeof(frame));

    switch (sequence % 4U)
    {
        case 0U:
            frame.id = 0x123U;
            frame.id_type = CAN_ID_STANDARD;
            frame.frame_type = CAN_FRAME_DATA;
            break;
        case 1U:
            frame.id = 0x01ABCDE0U;
            frame.id_type = CAN_ID_EXTENDED;
            frame.frame_type = CAN_FRAME_DATA;
            break;
        case 2U:
            frame.id = 0x321U;
            frame.id_type = CAN_ID_STANDARD;
            frame.frame_type = CAN_FRAME_REMOTE;
            break;
        default:
            frame.id = 0x01234567U;
            frame.id_type = CAN_ID_EXTENDED;
            frame.frame_type = CAN_FRAME_REMOTE;
            break;
    }

    frame.dlc = (uint8_t)(sequence % 9U);
    for (i = 0U; i < CAN_MAX_DLC; ++i)
    {
        frame.data[i] = (uint8_t)(sequence + ((uint32_t)i * 0x11U));
    }

    return frame;
}

/** @brief 比较两帧的 ID、格式、DLC 和有效数据。 */
static uint8_t frames_match(const can_frame_t *a, const can_frame_t *b)
{
    uint8_t i;

    if (a->id != b->id || a->id_type != b->id_type ||
        a->frame_type != b->frame_type || a->dlc != b->dlc)
    {
        return 0U;
    }

    if (a->frame_type == CAN_FRAME_REMOTE)
    {
        return 1U;
    }

    for (i = 0U; i < a->dlc; ++i)
    {
        if (a->data[i] != b->data[i])
        {
            return 0U;
        }
    }

    return 1U;
}

/** @brief 更新最近一次发送帧的调试信息。 */
static void record_tx(const can_frame_t *frame)
{
    g_can_test_stats.last_tx_id = frame->id;
    g_can_test_stats.last_tx_id_type = (uint8_t)frame->id_type;
    g_can_test_stats.last_tx_frame_type = (uint8_t)frame->frame_type;
    g_can_test_stats.last_tx_dlc = frame->dlc;
}

/** @brief 更新最近一次接收帧的调试信息。 */
static void record_rx(const can_frame_t *frame)
{
    uint8_t i;

    g_can_test_stats.last_rx_id = frame->id;
    g_can_test_stats.last_rx_id_type = (uint8_t)frame->id_type;
    g_can_test_stats.last_rx_frame_type = (uint8_t)frame->frame_type;
    g_can_test_stats.last_rx_dlc = frame->dlc;

    for (i = 0U; i < CAN_MAX_DLC; ++i)
    {
        g_can_test_stats.last_rx_data[i] = frame->data[i];
    }
}

/** @brief 清零统计信息并初始化 CAN BSP。 */
void App_CAN_Test_Init(void)
{
    can_status_t result;

    g_can_test_stats = (app_can_test_stats_t){0};
    s_expected_valid = 0U;
    s_sequence = 0U;
    s_last_tx_tick = GetTick_ms() - 100U;

    result = BSP_CAN_Init();
    g_can_test_stats.init_status = (int32_t)result;
    g_can_test_stats.initialized = (result == CAN_STATUS_OK) ? 1U : 0U;
}

/** @brief 执行一次轮询测试；每 100 ms 发送一帧并检查接收结果。 */
void App_CAN_Test_Task(void)
{
    const uint32_t now = GetTick_ms();
    can_frame_t received;
    can_error_status_t error_status;

    if (g_can_test_stats.initialized == 0U)
    {
        return;
    }

    if ((now - s_last_tx_tick) >= 100U)
    {
        can_frame_t outgoing = make_test_frame(s_sequence++);

        s_last_tx_tick = now;
        s_expected_valid = 0U;
        record_tx(&outgoing);

        if (BSP_CAN_Transmit(&outgoing, 20U) == CAN_STATUS_OK)
        {
            s_expected_frame = outgoing;
            s_expected_valid = 1U;
            g_can_test_stats.tx_ok++;
        }
        else
        {
            g_can_test_stats.tx_error++;
        }
    }

    while (BSP_CAN_Receive(&received, 0U) == CAN_STATUS_OK)
    {
        g_can_test_stats.rx_ok++;
        record_rx(&received);

        if (s_expected_valid != 0U &&
            frames_match(&s_expected_frame, &received) != 0U)
        {
            g_can_test_stats.rx_match++;
        }
        else
        {
            g_can_test_stats.rx_mismatch++;
        }

        s_expected_valid = 0U;
    }

    if (BSP_CAN_GetErrorStatus(&error_status) == CAN_STATUS_OK)
    {
        g_can_test_stats.tec = error_status.tec;
        g_can_test_stats.rec = error_status.rec;
        g_can_test_stats.error_warning = error_status.error_warning;
        g_can_test_stats.error_passive = error_status.error_passive;
        g_can_test_stats.bus_off = error_status.bus_off;
        g_can_test_stats.fifo0_overrun = error_status.fifo0_overrun;
    }
}

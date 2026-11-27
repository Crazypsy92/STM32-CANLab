#ifndef __APP_CAN_TEST_H
#define __APP_CAN_TEST_H

#include <stdint.h>

typedef struct
{
    volatile uint32_t initialized;
    volatile int32_t init_status;
    volatile uint32_t tx_ok;
    volatile uint32_t tx_error;
    volatile uint32_t rx_ok;
    volatile uint32_t rx_match;
    volatile uint32_t rx_mismatch;
    volatile uint32_t last_tx_id;
    volatile uint32_t last_rx_id;
    volatile uint8_t last_tx_ide;
    volatile uint8_t last_tx_rtr;
    volatile uint8_t last_tx_dlc;
    volatile uint8_t last_rx_ide;
    volatile uint8_t last_rx_rtr;
    volatile uint8_t last_rx_dlc;
    volatile uint8_t last_rx_data[8];
    volatile uint16_t tec;
    volatile uint16_t rec;
    volatile uint8_t error_warning;
    volatile uint8_t error_passive;
    volatile uint8_t bus_off;
    volatile uint8_t fifo0_overrun;
} app_can_test_stats_t;

extern volatile app_can_test_stats_t g_can_test_stats;

void App_CAN_Test_Init(void);
void App_CAN_Test_Task(void);

#endif /* __APP_CAN_TEST_H */

# Test Report

## Current status

| Item | Status | Evidence |
| --- | --- | --- |
| Repository initialization | DONE | STM32-CANLab repository created |
| Project skeleton migration | ADDED, NOT BUILD-VERIFIED | Source files copied from stm32f103-at24xx-i2c-bus and renamed |
| CAN frame model and bxCAN BSP | ADDED, NOT BUILD-VERIFIED | BSP headers and sources committed |
| Local loopback on STM32F103C8T6 | NOT TESTED | Requires board flashing and debugger observation |
| Physical CAN bus | NOT TESTED | Requires transceiver and a second active CAN node |
| CANH/CANL signal and termination | NOT TESTED | Requires physical bus measurement |

## First hardware acceptance

Use the default loopback mode and inspect `g_can_test_stats`:

- `initialized == 1`
- `tx_ok` increases
- `rx_ok` increases
- `rx_match` increases
- `tx_error == 0`
- `rx_mismatch == 0`

Do not mark physical CAN as passed based on internal loopback. Record the MCU board, transceiver model, second node/adapter, bitrate, termination and measured TX/RX counts when the physical test is performed.

## Known implementation boundaries

- CAN1, FIFO0 and filter bank 0 only.
- Polling implementation; interrupt-driven RX/TX is future work.
- Classical CAN 2.0A/B only; CAN FD is out of scope.
- Default bit timing assumes PCLK1 = 36 MHz.
- No UART console is enabled; inspect `g_can_test_stats` through SWD/debugger.

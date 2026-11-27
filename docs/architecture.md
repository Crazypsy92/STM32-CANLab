# CAN BSP Architecture

## Dependency direction

```
app_can_test
    |
    v
bsp_can (board configuration and public API)
    |
    v
can_bxcan (bxCAN controller backend)
    |
    v
STM32F1 CMSIS peripheral definitions / bxCAN registers
```

The CAN transceiver is an external physical-layer component between PB8/PB9 and CANH/CANL. It is not part of the bxCAN register driver.

## Layer responsibilities

### Frame and common types

`can_frame.h` contains the shared frame model and status types. It has no STM32 header dependency, so application code and future controller backends can use the same frame representation.

- `id_type`: standard 11-bit or extended 29-bit.
- `frame_type`: data or remote.
- `dlc`: 0..8 for Classical CAN.
- `data[8]`: meaningful for data frames; remote frames carry no data field.

### Controller backend

`can_bxcan.c` is the only layer that knows the bxCAN register layout. It handles initialization mode, bit timing, TX mailbox selection/completion, FIFO0 reception, filter bank 0 and error-state registers.

### Board support

`bsp_can.c` selects CAN1, PCLK1, PB8/PB9 remapping and the default bitrate. Changing the board pinout or bitrate should normally be done here rather than in the application.

### Application

`app_can_test.c` generates test frames, sends them and checks loopback results. It observes the public BSP API and exposes counters through `g_can_test_stats`.

## Current constraints

- CAN1 only.
- FIFO0 receive path only.
- Filter bank 0 only, 32-bit mask mode.
- Polling TX/RX; no CAN interrupts yet.
- Classical CAN only; no CAN FD.
- Initial bit timing assumes HSE=8 MHz, SYSCLK=72 MHz and PCLK1=36 MHz.

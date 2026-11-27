# Test Report

## Current status

| Item | Status | Evidence |
| --- | --- | --- |
| Repository initialization | DONE | STM32-CANLab repository created |
| Project skeleton migration | DONE | Source files copied from stm32f103-at24xx-i2c-bus and renamed |
| CAN frame model and bxCAN BSP | DONE | BSP headers and sources committed |
| CAN controller-independent error categories | DONE | bxCAN LEC is translated to common error categories |
| CAN peripheral clock lifecycle | DONE | BSP disables CAN1 clock after successful deinit and failed initialization |
| Public API Doxygen comments | DONE | Function parameters, return statuses, blocking behavior and structure fields documented |
| Protocol ID masks | DONE | `CAN_STD_ID_MASK`, `CAN_EXT_ID_MASK`, `CAN_MAX_DLC` |
| bxCAN register masks | DONE | CMSIS `*_Pos`, `*_Msk` and named mailbox flag table |
| Acceptance filter documentation | DONE | `id_mask` semantics and examples in `docs/can_filter_mask.md` |
| Coding conventions | DONE | `docs/coding_conventions.md` and `.clang-format` |
| ARM GCC Debug build | PASS | GitHub Actions Debug/Release workflow; 7,192 bytes Flash, 1,672 bytes RAM |
| ARM GCC Release build | PASS | GitHub Actions Debug/Release workflow; 3,872 bytes Flash, 1,672 bytes RAM |
| Local loopback on STM32F103C8T6 | NOT TESTED | Requires board flashing and debugger observation |
| Physical CAN bus | NOT TESTED | Requires transceiver and a second active CAN node |
| CANH/CANL signal and termination | NOT TESTED | Requires physical bus measurement |

## Build verification

The repository's GitHub Actions workflow successfully configured and built both Debug and Release with ARM GNU GCC 13.2.1, CMake and Ninja. [Run #42](https://github.com/Crazypsy92/STM32-CANLab/actions/runs/45951473291) completed without compiler warnings. The linker reported 7,192 B Flash / 1,672 B RAM for Debug and 3,872 B Flash / 1,672 B RAM for Release. These are build results only; they do not establish correct runtime behavior on the MCU.

## First hardware acceptance

Use the default silent internal loopback mode and inspect `g_can_test_stats`:

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

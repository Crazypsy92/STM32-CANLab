# STM32-CANLab

A bare-metal Classical CAN 2.0 test platform for STM32F103C8T6. The project uses STM32 LL for clock, GPIO and alternate-function setup, CMake/Ninja for building, and a small bxCAN register-level backend for CAN controller operations.

The initial firmware starts in **silent internal loopback mode**. It cycles through standard/extended data frames and standard/extended remote frames, with DLC values from 0 to 8. This checks the MCU-side controller path before connecting a physical CAN transceiver or a second node.

> Verification status: ARM GCC Debug and Release builds pass in GitHub Actions. Firmware flashing, on-board silent-loopback verification and physical CAN-bus validation are still pending. See [TEST_REPORT.md](TEST_REPORT.md).

## 1. Hardware baseline

| Item | Configuration |
| --- | --- |
| MCU | STM32F103C8T6, LQFP48 |
| Core clock | HSE 8 MHz -> PLL x9 -> 72 MHz |
| APB1 / CAN clock | PCLK1 = 36 MHz |
| CAN controller | bxCAN, CAN 2.0A/B |
| CAN pins | CAN1 partial remap 2: PB8 = RX, PB9 = TX |
| Default bitrate | 500 kbit/s |
| Driver style | Bare-metal + LL + CMSIS register access |
| Build | CMake + Ninja + arm-none-eabi-gcc |

The bxCAN bit timing is configured as:

- Prescaler = 4
- SJW = 1 TQ
- BS1 = 13 TQ
- BS2 = 4 TQ
- Total = 18 TQ
- Bitrate = 36 MHz / (4 x 18) = 500 kbit/s
- Sample point = 14 / 18 = 77.8%

The STM32F1 LL package does not provide a complete CAN peripheral LL API. Therefore the project uses LL for RCC/GPIO/remapping and CMSIS-defined bxCAN registers for controller configuration, transmit, receive, filters and error-state inspection. No HAL CAN driver is used.

## 2. Software layers

```
Application
  app_can_test.c
       |
Board support interface
  bsp_can.c / bsp_can.h
       |
Controller backend
  can_bxcan.c / can_bxcan.h
       |
STM32F1 bxCAN registers (CMSIS)
       |
External CAN transceiver (physical layer, not part of MCU firmware)
```

| File | Responsibility |
| --- | --- |
| `can_frame.h` | MCU-independent Classical CAN frame, ID masks, acceptance-filter types, status and error types |
| `can_bxcan.c/h` | bxCAN init, bit timing, CMSIS register masks, TX mailboxes, RX FIFO0, 32-bit ID-mask filter and error status |
| `bsp_can.c/h` | CAN1 clock, PB8/PB9 remap, board timing and public BSP entry points |
| `app_can_test.c/h` | Loopback test traffic and debugger-visible counters |
| `delay.c/h` | Shared SysTick millisecond timebase and DWT microsecond timebase |

The application uses only the BSP interface and frame types. It does not include bxCAN register definitions or access MCU registers directly.

## 3. Current feature scope

- Standard 11-bit and extended 29-bit identifiers.
- Data frames and remote frames.
- DLC validation from 0 to 8.
- Three TX mailboxes and polling completion status.
- RX FIFO0 polling.
- One 32-bit identifier mask filter in filter bank 0; standard/extended IDE matching or accept-all mode.
- Error warning, error passive, bus-off, TEC/REC and FIFO0 overrun status.
- Normal, loopback, silent and silent-loopback controller modes.
- Automatic bus-off management enabled; automatic retransmission disabled by default for bounded test behavior.

This is Classical CAN only. CAN FD is not supported. The first implementation uses polling; interrupt-driven RX/TX and multiple filter banks can be added in later stages.

## 3.1 Public API and coding style

Public CAN functions and data structures are documented with Doxygen comments in their headers. The project uses named identifier masks (`CAN_STD_ID_MASK`, `CAN_EXT_ID_MASK`) and CMSIS register field definitions (`*_Pos`, `*_Msk`) rather than unexplained numeric limits and register shifts.

The common frame and filter types are independent of STM32 headers. The `can_bxcan` backend remains STM32 bxCAN-specific, while `bsp_can` owns this board's clocks, pins and bit timing. See [coding conventions](docs/coding_conventions.md) and [acceptance filter mask examples](docs/can_filter_mask.md).

A typical filter for standard IDs `0x120` through `0x12F` is:

```c
const can_filter_t filter = {
    .id = 0x120U,
    .id_mask = 0x7F0U,
    .id_type = CAN_ID_STANDARD
};

(void)BSP_CAN_SetFilter(&filter);
```

For `id_mask`, a bit value of 1 means compare the corresponding ID bit; 0 means ignore it.

## 4. Default silent loopback test

The default is selected in `BSP/Inc/bsp_can.h`:

```c
#define BSP_CAN_DEFAULT_MODE BSP_CAN_MODE_SILENT_LOOPBACK
```

The application sends one frame every 100 ms and cycles through:

1. Standard data frame, ID `0x123`.
2. Extended data frame, ID `0x01ABCDE0`.
3. Standard remote frame, ID `0x321`.
4. Extended remote frame, ID `0x01234567`.

DLC cycles from 0 through 8. The application compares the received IDE, ID, RTR/frame type, DLC and data bytes (data bytes are not compared for remote frames).

Watch `g_can_test_stats` in the debugger:

| Field | Meaning |
| --- | --- |
| `initialized` / `init_status` | BSP initialization result |
| `tx_ok` / `tx_error` | Successful / failed transmit requests |
| `rx_ok` | Frames read from FIFO0 |
| `rx_match` / `rx_mismatch` | Exact loopback comparison result |
| `last_tx_*` / `last_rx_*` | Last frame fields |
| `tec` / `rec` | Hardware transmit / receive error counters |
| `error_warning` / `error_passive` / `bus_off` | Controller error state |

For a healthy internal loopback run, `tx_ok`, `rx_ok` and `rx_match` should increase together, while `tx_error` and `rx_mismatch` remain zero.

Silent internal loopback keeps the physical CAN TX output recessive. It does **not** verify the transceiver, CANH/CANL wiring, termination, physical ACK, electrical signal quality or real-bus arbitration.

## 5. Physical CAN bus

For physical communication, change the default mode to normal:

```c
#define BSP_CAN_DEFAULT_MODE BSP_CAN_MODE_NORMAL
```

Then connect PB9 to the transceiver TXD input and PB8 to the transceiver RXD output. Connect the transceiver CANH/CANL pins to the bus. Do not connect MCU GPIO pins directly to CANH/CANL.

A physical bus needs at least two active CAN nodes at the same bitrate. A single normal-mode node without another node to acknowledge frames will report transmit errors. See [docs/hardware_wiring.md](docs/hardware_wiring.md) before wiring.

## 6. Build

Requirements:

- CMake 3.22+
- Ninja
- `arm-none-eabi-gcc` toolchain
- OpenOCD for ST-Link flashing (optional)

```bash
cmake --preset Debug
cmake --build --preset Debug

# Optional Intel HEX output
arm-none-eabi-objcopy -O ihex build/Debug/stm32_canlab.elf build/Debug/stm32_canlab.hex
```

Flash with ST-Link and OpenOCD:

```bash
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "program build/Debug/stm32_canlab.elf verify reset exit"
```

The linker script is configured for STM32F103C8Tx (64 KB Flash / 20 KB SRAM). If the actual MCU variant has a different memory size, update the linker script and device definition deliberately.

## 7. Repository layout

```
STM32-CANLab/
├── BSP/
│   ├── Inc/
│   │   ├── app_can_test.h
│   │   ├── bsp_can.h
│   │   ├── bsp_includes.h
│   │   ├── can_bxcan.h
│   │   ├── can_frame.h
│   │   └── delay.h
│   └── Src/
│       ├── app_can_test.c
│       ├── bsp_can.c
│       ├── can_bxcan.c
│       └── delay.c
├── Core/
│   ├── Inc/
│   └── Src/
├── Drivers/
│   ├── CMSIS/
│   └── STM32F1xx_HAL_Driver/   # LL headers and selected LL sources only
├── cmake/
├── docs/
├── STM32-CANLab.ioc
├── STM32F103C8Tx_FLASH.ld
├── CMakeLists.txt
└── CMakePresets.json
```

The AT24xx, I2C, OLED and button application modules were intentionally not copied: they are unrelated to this CAN project. The reusable startup, CMSIS, LL, timebase, CMake and toolchain foundation was retained and renamed for this repository.

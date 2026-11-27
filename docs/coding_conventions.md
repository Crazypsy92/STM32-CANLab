# CAN Driver Coding Conventions

This document defines the coding rules used by the CAN driver and its public interfaces.

## 1. File and symbol naming

- Source and header files use lower snake case: `can_bxcan.c`, `bsp_can.h`.
- Public functions carry a module prefix: `CAN_BxCAN_*`, `BSP_CAN_*`.
- Private functions and file-local state use `static` and lower snake case.
- Macros and enum constants use upper snake case.
- Public types use a module-specific suffix, such as `can_frame_t`.

## 2. Public API documentation

Every public function declaration must have a Doxygen block immediately above it.

Use:
- `@brief` for one-line behavior.
- `@param[in]`, `@param[out]`, and `@param[in,out]` for parameter direction.
- `@return` and `@retval` for result semantics.
- `@note` for blocking behavior, timing requirements, side effects and hardware limitations.

Every public structure member and non-obvious enum value should have a short field comment. Private helpers should have a concise comment when their behavior is not self-evident.

## 3. Identifier masks

Use named protocol masks rather than repeating numeric ID limits:

| Macro | Value | Meaning |
| --- | ---: | --- |
| `CAN_STD_ID_MASK` | `0x7FFU` | Valid 11-bit standard identifier |
| `CAN_EXT_ID_MASK` | `0x1FFFFFFFU` | Valid 29-bit extended identifier |
| `CAN_MAX_DLC` | `8U` | Maximum Classical CAN payload length |

For acceptance filters, `id_mask` follows the common CAN mask convention:

- Mask bit = 1: the corresponding ID bit must match.
- Mask bit = 0: the corresponding ID bit is ignored.

Example: standard ID `0x120`, mask `0x7F0` accepts IDs `0x120` through `0x12F`.

## 4. Register bit operations

- Prefer CMSIS device-header definitions such as `CAN_MCR_INRQ`, `CAN_TI0R_IDE`, `CAN_BTR_BRP_Msk` and `CAN_BTR_BRP_Pos`.
- Use `*_Msk` / `*_Pos` for register fields instead of unexplained shifts or hexadecimal literals.
- Use a named local mask or a small descriptor table when register bits repeat across multiple hardware instances, such as the three bxCAN TX mailboxes.
- Keep register-level code inside the controller backend. Application code must use the BSP API.

## 5. Layering and portability

- `can_frame.h` contains protocol-level types and controller-independent error categories; it must not expose peripheral register encodings or include MCU headers.
- `can_bxcan.*` is STM32 bxCAN-specific and may include CMSIS device definitions.
- `bsp_can.*` owns board-specific clocks, pins and bit timing.
- `app_can_test.*` demonstrates use of the BSP; it must not access peripheral registers.
- Do not claim a driver is hardware-independent when it depends on a specific controller backend.

## 6. Current scope

The current backend supports Classical CAN, one 32-bit mask filter in Filter Bank 0, FIFO0 and polling TX/RX. Interrupt-driven operation, multiple filter banks, CAN FD and additional MCU backends are separate future work.


## 7. Data types and state

- Use `uint8_t`, `uint16_t` and `uint32_t` for fields with a defined protocol or register width.
- For public structures shared across toolchains, prefer fixed-width storage fields plus named enum constants over compiler-sized enum members.
- Use an explicit state enum when a module has distinct lifecycle states. Do not use a Boolean to represent more than two lifecycle conditions.
- Update state only after the operation succeeds or fails; do not report a hardware transition before it is confirmed.
- Apply `volatile` at the shared object boundary when required by asynchronous observation. Do not mark every structure member volatile by default.

# CAN Acceptance Filter Mask

The CAN acceptance filter decides which received frames are forwarded by the controller to a receive FIFO.

## Mask meaning

The `id_mask` field uses the conventional bit-mask rule:

- `1`: compare this identifier bit with the corresponding bit in `id`.
- `0`: ignore this identifier bit.

This is the same semantic convention used by the Zephyr CAN filter API. ST HAL exposes the same concept through fields such as `FilterIdHigh` and `FilterMaskIdHigh`.

## Example: accept standard IDs 0x120..0x12F

```c
const can_filter_t filter = {
    .id = 0x120U,
    .id_mask = 0x7F0U,
    .id_type = CAN_ID_STANDARD
};

(void)BSP_CAN_SetFilter(&filter);
```

The 11-bit mask `0x7F0` compares the upper seven identifier bits and ignores the lower four bits:

```
ID:    001 0010 0000   (0x120)
Mask:  111 1111 0000   (0x7F0)
                         ^^^^
                         ignored
```

Therefore IDs from `0x120` through `0x12F` match.

## Accept all identifiers

```c
const can_filter_t filter = {
    .id = 0U,
    .id_mask = 0U,
    .id_type = CAN_ID_ANY
};

(void)BSP_CAN_SetFilter(&filter);
```

## Current backend limitations

- Only Filter Bank 0 is configured.
- Filter scale is fixed to 32-bit.
- Filter mode is fixed to identifier-mask mode.
- Accepted frames are routed to FIFO0.
- Standard/extended format is matched when `id_type` is STANDARD or EXTENDED.
- RTR is not part of the current public filter description; matching IDs can pass as either data or remote frames.

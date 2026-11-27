# CAN Test Plan

## Stage 0 — Build and local loopback

1. Configure and build the Debug preset.
2. Flash the STM32F103C8T6.
3. Keep `BSP_CAN_DEFAULT_MODE` set to silent loopback.
4. Watch `g_can_test_stats` in the debugger.
5. Confirm that TX, RX and exact-match counters advance, with no mismatch.

Coverage in the current test application:
- Standard data frame.
- Extended data frame.
- Standard remote frame.
- Extended remote frame.
- DLC values 0..8.
- Exact ID, IDE, RTR, DLC and data comparison (data ignored for remote frames).

## Stage 1 — Physical two-node communication

1. Add a compatible CAN transceiver to the STM32 node.
2. Connect a second node or USB-CAN adapter.
3. Configure both nodes for 500 kbit/s.
4. Add two 120 ohm termination resistors at the bus ends.
5. Change the firmware mode to normal and rebuild.
6. Verify TX/RX data and inspect TEC/REC.

Do not interpret an ACK error from a single normal-mode node as a firmware failure: no other node is present to acknowledge the frame.

## Stage 2 — Acceptance filtering

1. Configure a standard-ID mask filter.
2. Send one matching standard frame and one non-matching standard frame.
3. Configure an extended-ID mask filter.
4. Verify IDE matching and FIFO0 delivery.
5. Restore accept-all filter.

## Stage 3 — Arbitration and error confinement

1. Use at least two transmitters with different identifiers.
2. Request transmission at overlapping times.
3. Confirm that the lower numerical arbitration field wins without corrupting the winning frame.
4. Create controlled ACK/error conditions.
5. Record TEC/REC and error-state transitions.
6. Verify bus-off indication and automatic recovery behavior.

Arbitration waveform, ACK, bit timing margin, physical faults and actual bus-off recovery require a real bus and suitable second node/analyzer. They are not proven by internal loopback.

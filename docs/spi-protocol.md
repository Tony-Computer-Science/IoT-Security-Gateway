# SPI inter-board protocol

**Version: 0.1.0 (DRAFT: physical layer pins defined, protocol not yet designed)**

This document will define the only communication path between Board A
(station, SPI **controller**) and Board B (bridge, SPI **peripheral**). Board A's
`spi_host` component implements one side and Board B's `spi_device` component
the other.

Everything marked **TODO** is undecided. Do not implement against guesses:
resolve the TODO in this document first, through a PR reviewed by both sides of
the link (see [CONTRIBUTING.md](../CONTRIBUTING.md#changes-to-shared-contracts)).

---

## 1. Physical layer

| Item | Value |
|---|---|
| Signals | SCLK, MOSI, MISO, CS, plus one handshake line |
| Max clock | ≤ 10 MHz |
| Exact clock | TODO |
| SPI mode (CPOL/CPHA) | TODO |
| Bit order | TODO |
| Pin assignments, Board A | SCLK GPIO12, MOSI GPIO11, MISO GPIO13, CS GPIO10 (the SPI2 IO_MUX pins), HS GPIO4, see `hardware/docs/` |
| Pin assignments, Board B | Same GPIO numbers as Board A, see `hardware/docs/` |
| Handshake line direction and meaning | Driven by Board B, input on Board A. Active high: B raises HS when it has data for A, and A then starts the transfer, so every transfer is initiated by A. Idles low (10 kΩ pull-down at A). TODO: exact timing (when B releases HS) |
| Cable length and termination | PCB: ~75 mm traces, 33 Ω series at the driver (A on SCLK/MOSI/CS, B on MISO/HS); CS idles high (10 kΩ pull-up at B). Breadboard: TODO, see `hardware/` |

## 2. Frame format

TODO: define the byte layout. Questions it must answer:

- Fixed-size or length-prefixed frames? What is the maximum frame size?
- Magic or start-of-frame marker?
- Protocol version field?
- Message type field and its allowed values?
- Request ID, so a response can be matched to its request and stale responses
  after a restart can be rejected (R7)?
- Integrity check (CRC-16 / CRC-32)? Which polynomial?
- Byte order for multi-byte fields?

| Offset | Size | Field | Notes |
|---|---|---|---|
| TODO | | | |

## 3. Message types

TODO: list every message. The minimum needed by the REST contract:

| Type | Direction | Purpose | Payload |
|---|---|---|---|
| TODO | A → B | Set output (on/off) | TODO |
| TODO | A → B | Read status | TODO |
| TODO | B → A | Result (ok + output state) | TODO |
| TODO | B → A | Error (target unreachable, invalid frame) | TODO |
| TODO | both | Link heartbeat / ready | TODO |

Board B must reject any type that isn't in this table (R3).

## 4. Validation rules

TODO: what each side checks before acting on a frame (length, type, CRC,
version, request ID, payload values), and what it does with a bad frame:
drop it, reply with an error, or count it toward a link fault.

## 5. Link state and readiness (R7)

TODO:

- How each side decides the link is **ready**: handshake sequence at boot.
- How a restart on either side is detected (e.g. boot nonce or epoch counter).
- What happens to in-flight requests when a restart is detected. They must be
  invalidated, never replayed.
- Heartbeat interval and timeout before `link_ok` becomes false.

## 6. Timing

TODO: per-transaction timeout on Board A, and Board B's HTTP timeout to the
target. Board A's timeout must be longer than Board B's, so Board A can report
`target_unreachable` (502) rather than `link_down` (503).

## 7. Changelog

- 0.1.0: §1 physical layer: pin assignments, handshake direction and idle state, and termination defined by the PCB (Rev A). Everything else unchanged.
- 0.0.0: stub created. Nothing decided.

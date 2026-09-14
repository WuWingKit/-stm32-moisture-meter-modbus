# STM32 Moisture Meter Communication System over Modbus

[中文](./README.zh-CN.md) · [Experiment report](./docs/experiment-report.docx) · [Presentation](./presentation/project-presentation.pptx)

[![STM32F103](https://img.shields.io/badge/MCU-STM32F103RC-03234B?logo=stmicroelectronics)](https://www.st.com/en/microcontrollers-microprocessors/stm32f103rc.html)
[![Modbus](https://img.shields.io/badge/Protocol-Modbus_RTU_over_TCP-1F6FEB)](https://modbus.org/)
[![C](https://img.shields.io/badge/Language-C-A8B9CC?logo=c)](https://www.iso.org/standard/82075.html)
[![Keil](https://img.shields.io/badge/IDE-Keil_MDK-394049)](https://www.keil.com/)
[![License: CC BY-NC-SA 4.0](https://img.shields.io/badge/Archive-CC_BY--NC--SA_4.0-lightgrey.svg)](./LICENSE-CONTENT.md)

An end-to-end industrial communication experiment in which an STM32F103RC simulates a moisture meter Modbus slave, an EBYTE NT1-B transparently bridges UART and Ethernet, and MThings acts as the supervisory master.

![System architecture](./assets/system-architecture.png)

## Project at a glance

| Item | Details |
|---|---|
| Project period | **April 2026** (final report dated 30 April 2026) |
| Project lead | **Hu Rongjie (胡荣杰)** |
| Actual developer | **Hu Rongjie — sole designer and developer** |
| Contribution clarification | Any other names associated with the course group were nominal only. Firmware, protocol implementation, networking, hardware integration, debugging, validation, presentation, and report were completed by **Hu Rongjie alone**. |
| Project type | Embedded firmware + industrial protocol + network integration experiment |
| Prototype cost | **Not recorded in the supplied materials — to be added** |
| Completion status | FC03/FC06, network bridge, live host monitoring, and read/write validation completed |

## Commercialization and application analysis

This prototype demonstrates a low-cost retrofit path for legacy serial instruments: keep the RTU device firmware and place a transparent serial-to-Ethernet bridge in front of it. The pattern can be applied to moisture meters, weighing instruments, environmental sensors, laboratory rigs, and older factory equipment that must be connected to a local monitoring system without redesigning the controller.

For a commercial product, the simulated values must be replaced by calibrated sensor acquisition; RS-485 isolation, surge/ESD protection, watchdog recovery, proper Modbus exception responses, configuration security, device provisioning, and long-duration reliability tests are also required. The current system is best understood as a validated communication prototype, not a production moisture instrument.

## Architecture and protocol

```text
MThings master
    │ TCP/IP
Router / LAN
    │ TCP, port 502
EBYTE NT1-B (transparent bridge)
    │ UART, 9600 bps, 8N1
STM32F103RC Modbus RTU slave, address 0x01
```

The transport is **Modbus RTU over TCP**, not native Modbus TCP. The NT1-B does not translate Modbus frames; it forwards bytes unchanged. Selecting native Modbus TCP adds a 7-byte MBAP header, so the STM32 sees `0x00` instead of slave address `0x01` and rejects the frame. Switching MThings to RTU over TCP solved the end-to-end failure.

### Implemented register map

| Address | Register | Access | Initial/raw value | Host display |
|---|---|---|---:|---:|
| `0x0010` | Aggregate/stone type | Read/write | `1` | `1` |
| `0x0011` | Simulated moisture | Read-only | `128` | `12.8%` |
| `0x0012` | Simulated temperature | Read-only | `253` | `25.3 °C` |

Moisture and temperature are stored as value × 10 and updated once per second using bounded pseudo-random walks. They are **simulated signals**, not readings from a physical moisture sensor.

### Firmware behavior

- USART2 receives RTU frames at `9600 bps, 8N1` through an interrupt-driven buffer.
- The foreground loop waits 20 ms after reception, validates address and CRC16-Modbus, then dispatches the request.
- Function `0x03` reads holding registers; function `0x06` writes a single allowed register and echoes the request.
- Moisture and temperature registers reject FC06 writes.
- USART1 outputs startup status, sensor values, RX/TX frames, and register operations at `115200 bps`.

![Validated host-side results](./assets/experiment-results.png)

## Verified results

- MThings read all three registers through the complete Ethernet/UART chain.
- Writing value `3` to `0x0010` returned the required FC06 echo and was confirmed by a subsequent read.
- Writing to read-only `0x0011` was rejected and caused a host timeout.
- Live moisture and temperature curves refreshed from the simulated values.
- Debug logs exposed raw hexadecimal request and response frames.

## Build and reproduce

1. Open `USER/Moisture_Meter.uvprojx` in Keil MDK 5 and build the STM32F103RC target.
2. Flash with ST-Link.
3. Connect STM32 `PA2 (TX)` to NT1-B `RXD`, `PA3 (RX)` to `TXD`, and share ground.
4. Configure NT1-B as TCP Server, serial `9600 8N1`, with an address reachable from the host LAN.
5. In MThings select **Modbus RTU over TCP**, slave address `1`, and add addresses 16–18.

The archived report records the original lab address `192.168.0.10:502`; choose an appropriate non-conflicting address on your own network.

## Repository contents

- `USER/main.c`: protocol, registers, sensor simulation, and dual-UART logic
- `docs/experiment-report.docx`: complete original experiment report
- `presentation/project-presentation.pptx`: original 44-slide presentation
- `assets/`: README images exported from that presentation

## Limitations and next steps

- No physical moisture sensor is connected.
- Frame delimiting uses a fixed 20 ms wait rather than a timer/idle-line state machine.
- Invalid writes are silently dropped instead of returning standard Modbus exception frames.
- FC16, native Modbus TCP, RS-485 multi-drop, authentication, and cloud telemetry are not implemented.

## License

Project-authored documentation, presentation, and images are shared under **CC BY-NC-SA 4.0**; see [LICENSE-CONTENT.md](./LICENSE-CONTENT.md). Source and third-party vendor components retain the terms stated in their respective files.

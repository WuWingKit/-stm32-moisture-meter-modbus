# Moisture Meter Modbus Slave (STM32F103RC)

A complete embedded system project that simulates a **soil moisture meter Modbus RTU slave** using STM32F103RC, communicating with a host PC over Ethernet via an NT1-B serial-to-Ethernet module.

## System Architecture

```
MThings / Modbus Poll (PC, Modbus Master)
    ↕ TCP/IP (Ethernet)
Router (192.168.0.1)
    ↕ Ethernet
NT1-B Serial-to-Ethernet Module (TCP Server, IP: 192.168.0.10:502)
    ↕ UART Serial (9600bps, 8N1)
STM32F103RC (Modbus Slave, Address 0x01)
```

## Features

- **Modbus RTU Slave** — Supports Function Code 03 (Read Registers) and 06 (Write Single Register)
- **Dynamic Sensor Simulation** — Moisture and temperature values update every second with realistic random walk
- **Dual Serial Ports** — USART2 for Modbus communication, USART1 for real-time debug output (printf)
- **NT1-B Transparent Forwarding** — STM32 processes only serial RTU frames, no TCP/IP stack needed
- **Read-Only Protection** — Moisture and temperature registers reject write operations

## Register Map

| Address | Name | Type | Initial | Description |
|---------|------|------|---------|-------------|
| 0x0000 | Address Setting | R/W | 1 | Slave address |
| 0x0001 | Moisture Parameter | R/W | 0 | Parameter setting |
| 0x0010 | Stone Type | R/W | 1 | 1=coarse sand, 2=fine sand... |
| 0x0011 | Moisture Value | Read-only | 128 | ×10, range 50~200 (5.0%~20.0%) |
| 0x0012 | Temperature | Read-only | 253 | ×10, range 150~350 (15.0℃~35.0℃) |

## Project Structure

```
Moisture meter/
├── CORE/                    # ARM CMSIS core + startup
│   ├── core_cm3.c/h
│   └── startup_stm32f10x_hd.s
├── STM32F10x_FWLib/         # ST Standard Peripheral Library
│   ├── inc/                 (22 headers)
│   └── src/                 (22 sources)
├── SYSTEM/                  # System utilities (Alientek framework)
│   ├── delay/               delay_init, delay_ms, delay_us
│   ├── sys/                 Bit-band operations, system config
│   └── usart/               USART1 init + printf redirect
├── USER/
│   ├── main.c               # ★ Main: Modbus slave logic + sensor simulation
│   ├── stm32f10x_it.c/h     # Interrupt handlers
│   ├── stm32f10x_conf.h     # Peripheral configuration
│   ├── system_stm32f10x.c/h # System clock init
│   └── Moisture_Meter.uvprojx # Keil5 project file
└── OBJ/                     # Build output (Moisture_Meter.hex)
```

## Hardware Requirements

| Device | Model | Purpose |
|--------|-------|---------|
| MCU | STM32F103RC | Modbus slave controller |
| Serial-Ethernet Module | EBYTE NT1-B | Transparent UART↔TCP forwarding |
| Router | Any | LAN for NT1-B and PC |
| TTL-USB | CH340/CP2102 | Debug output (USART1, 115200bps) |
| Programmer | ST-Link V2 | Flash/debug |

## Wiring

**STM32 ↔ NT1-B (Modbus, USART2):**
```
PA2 (TX)  →  RXD
PA3 (RX)  ←  TXD
GND       →  GND
3.3V      →  VCC
```

**STM32 ↔ TTL-USB (Debug, USART1):**
```
PA9 (TX)  →  RXD
GND       →  GND
```

**NT1-B ↔ Router ↔ PC:**
```
NT1-B LAN port ──ethernet──→ Router ──ethernet──→ PC
```

## NT1-B Configuration

Configure via EBYTE Network Configuration Tool or web interface:

| Parameter | Value |
|-----------|-------|
| Work Mode | TCP Server |
| Local IP | 192.168.0.10 |
| Local Port | 502 |
| Baud Rate | 9600 |
| Data Bits / Stop Bits / Parity | 8 / 1 / None |

## Building

1. Open `USER/Moisture_Meter.uvprojx` in Keil MDK-ARM V5
2. Ensure compiler defines: `STM32F10X_HD,USE_STDPERIPH_DRIVER`
3. Build (F7) → output hex to `OBJ/Moisture_Meter.hex`
4. Flash via ST-Link

## Debug Output

Connect TTL-USB to USART1 (PA9, 115200bps):

```
=== Moisture Meter Modbus Slave ===
Slave Addr: 0x01
USART2: 9600 bps (Modbus)
USART1: 115200 bps (Debug)
Waiting for Modbus requests...

[SENSOR] Moisture=12.8%  Temp=25.3C
[RX] 8 bytes: 01 03 00 11 00 01 D5 CA
FC03: read reg 0x0011, count 1
[TX] 7 bytes: 01 03 02 00 80 B9 FC
[SENSOR] Moisture=13.1%  Temp=25.2C
```

## MThings / Modbus Poll Setup

**⚠️ Important:** Select **"Modbus RTU over TCP"** (NOT "Modbus TCP").

| Parameter | Value |
|-----------|-------|
| Protocol | Modbus RTU over TCP |
| IP Address | 192.168.0.10 |
| Port | 502 |
| Slave Address | 1 |

**Why not Modbus TCP?** The NT1-B is in transparent forwarding mode. Modbus TCP frames have a 7-byte MBAP header instead of the slave address as the first byte, so the STM32 address check fails.

## Modbus RTU vs TCP over NT1-B

```
Modbus RTU (works):     [Addr][FC][Data][CRC]
Modbus TCP (fails):     [MBAP:7B][UnitID][FC][Data]   ← first byte = 0x00, not 0x01
RTU over TCP (works):   [Addr][FC][Data][CRC]          ← same as RTU, over TCP
```

## CRC16-Modbus

Generator polynomial: `0x8005` (reversed: `0xA001`). Initial value: `0xFFFF`. Low byte sent first.

## License

MIT License — see [LICENSE](LICENSE) for details.

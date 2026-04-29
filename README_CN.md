# 水分仪 Modbus 从机模拟系统 (STM32F103RC)

基于 STM32F103RC 的嵌入式实验项目，模拟**土壤水分仪 Modbus RTU 从机**，通过 NT1-B 串口转网口模块与上位机进行以太网通信。

## 系统架构

```
MThings / Modbus Poll（上位机，Modbus 主机）
    ↕ TCP/IP 以太网
路由器（192.168.0.1）
    ↕ 以太网
NT1-B 串口转网口模块（TCP Server，IP: 192.168.0.10:502）
    ↕ UART 串口（9600bps, 8N1）
STM32F103RC（Modbus 从机，地址 0x01）
```

## 功能特性

- **Modbus RTU 从机** — 支持功能码 03（读寄存器）和 06（写单个寄存器）
- **动态传感器模拟** — 水分值和温度值每秒更新，模拟真实传感器的随机游走变化
- **双串口设计** — USART2 用于 Modbus 通信，USART1 用于实时调试输出（printf）
- **NT1-B 透明转发** — STM32 只处理串口 RTU 帧，无需 TCP/IP 协议栈
- **读写保护** — 水分值和温度值寄存器为只读，拒绝写操作

## 寄存器表

| 地址 | 名称 | 类型 | 初始值 | 说明 |
|------|------|------|--------|------|
| 0x0000 | 水分仪地址设置 | 读写 | 1 | 从机地址 |
| 0x0001 | 水分参数设置 | 读写 | 0 | 参数配置 |
| 0x0010 | 砂石种类 | 读写 | 1 | 1=粗砂，2=细砂… |
| 0x0011 | 实时水分值 | 只读 | 128 | ×10，范围 50~200（5.0%~20.0%） |
| 0x0012 | 温度值 | 只读 | 253 | ×10，范围 150~350（15.0℃~35.0℃） |

## 工程目录结构

```
Moisture meter/
├── CORE/                    # ARM CMSIS 内核文件 + 启动文件
│   ├── core_cm3.c/h
│   └── startup_stm32f10x_hd.s
├── STM32F10x_FWLib/         # STM32 标准外设库
│   ├── inc/                 （22个头文件）
│   └── src/                 （22个源文件）
├── SYSTEM/                  # 系统工具模块（正点原子框架）
│   ├── delay/               delay_init, delay_ms, delay_us
│   ├── sys/                 位带操作、系统配置
│   └── usart/               USART1 初始化 + printf 重定向
├── USER/
│   ├── main.c               # ★ 主程序：Modbus 从机逻辑 + 传感器模拟
│   ├── stm32f10x_it.c/h     # 中断服务函数
│   ├── stm32f10x_conf.h     # 外设配置
│   ├── system_stm32f10x.c/h # 系统时钟初始化
│   └── Moisture_Meter.uvprojx # Keil5 工程文件
└── OBJ/                     # 编译输出（Moisture_Meter.hex）
```

## 硬件清单

| 设备 | 型号 | 用途 |
|------|------|------|
| 主控 | STM32F103RC | Modbus 从机控制器 |
| 串口转网口模块 | EBYTE NT1-B | UART↔TCP 透明转发 |
| 路由器 | 任意 | 为 NT1-B 和电脑提供局域网 |
| TTL-USB 模块 | CH340/CP2102 | 调试输出（USART1, 115200bps） |
| 烧录器 | ST-Link V2 | 程序烧录与调试 |

## 接线方式

**STM32 ↔ NT1-B（Modbus 通信，USART2）：**
```
PA2 (TX)  →  RXD
PA3 (RX)  ←  TXD
GND       →  GND
3.3V      →  VCC
```
注意：TX 接 RX，RX 接 TX，交叉连接！

**STM32 ↔ TTL-USB 模块（调试输出，USART1）：**
```
PA9 (TX)  →  RXD
GND       →  GND
```

**NT1-B ↔ 路由器 ↔ 电脑：**
```
NT1-B 网口 ──网线──→ 路由器 ──网线──→ 电脑
```

## NT1-B 模块配置

通过 EBYTE 网络配置工具或网页端配置：

| 参数 | 设置值 |
|------|--------|
| 工作模式 | TCP Server |
| 本地 IP | 192.168.0.10 |
| 本地端口 | 502 |
| 串口波特率 | 9600 |
| 数据位/停止位/校验 | 8 / 1 / 无 |

## 编译

1. 用 Keil MDK-ARM V5 打开 `USER/Moisture_Meter.uvprojx`
2. 确认编译宏：`STM32F10X_HD,USE_STDPERIPH_DRIVER`
3. Build（F7）→ 输出 hex 到 `OBJ/Moisture_Meter.hex`
4. 通过 ST-Link 烧录

## 调试输出

将 TTL-USB 模块连接到 USART1（PA9, 115200bps），打开串口助手：

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

## MThings / Modbus Poll 配置

**⚠️ 重要：** 协议类型必须选择 **"Modbus RTU over TCP"**（不是 "Modbus TCP"）。

| 参数 | 设置值 |
|------|--------|
| 协议类型 | Modbus RTU over TCP |
| IP 地址 | 192.168.0.10 |
| 端口 | 502 |
| 从机地址 | 1 |

### 为什么不能用 Modbus TCP？

NT1-B 在透明转发模式下，把网络数据原样转发给 STM32：

```
Modbus RTU 帧（可用）：   [从机地址][功能码][数据][CRC]
Modbus TCP 帧（不可用）： [MBAP头:7字节][单元ID][功能码][数据]   ← 第一个字节是 0x00，不是从机地址 0x01
RTU over TCP（可用）：    [从机地址][功能码][数据][CRC]          ← 和串口 RTU 一样，只是走 TCP
```

STM32 代码按 RTU 格式解析（第一个字节=从机地址），收到 TCP 帧的第一个字节是 MBAP 头的事务 ID 高字节（通常为 `0x00`），从机地址校验失败，帧被丢弃。

## CRC16-Modbus 算法

生成多项式：`0x8005`（反转形式：`0xA001`）。初始值：`0xFFFF`。结果低字节先发。

```c
uint16_t CRC16_Modbus(uint8_t *buf, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    for(i = 0; i < len; i++) {
        crc ^= buf[i];
        for(j = 0; j < 8; j++) {
            if(crc & 0x0001)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }
    return crc;  // 低字节在前，高字节在后
}
```

## 调试顺序建议

1. 烧录程序，用串口助手连接 USART1（115200bps），确认启动信息和传感器数据输出
2. 用串口助手连接 USART2（9600bps），手动发送 `01 03 00 11 00 01 D5 CA`，确认应答
3. 接入 NT1-B 模块，浏览器访问配置页确认参数
4. ping 192.168.0.10 确认网络连通
5. 打开 MThings，新建 Modbus RTU over TCP 连接，读寄存器测试

## 许可证

MIT License — 详见 [LICENSE](LICENSE)。

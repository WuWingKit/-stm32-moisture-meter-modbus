# 基于 Modbus 协议的 STM32 水分仪通信系统

[English](./README.md) · [实验报告](./docs/experiment-report.docx) · [项目汇报](./presentation/project-presentation.pptx)

[![STM32F103](https://img.shields.io/badge/MCU-STM32F103RC-03234B?logo=stmicroelectronics)](https://www.st.com/en/microcontrollers-microprocessors/stm32f103rc.html)
[![Modbus](https://img.shields.io/badge/Protocol-Modbus_RTU_over_TCP-1F6FEB)](https://modbus.org/)
[![C](https://img.shields.io/badge/Language-C-A8B9CC?logo=c)](https://www.iso.org/standard/82075.html)
[![Keil](https://img.shields.io/badge/IDE-Keil_MDK-394049)](https://www.keil.com/)
[![License: CC BY-NC-SA 4.0](https://img.shields.io/badge/Archive-CC_BY--NC--SA_4.0-lightgrey.svg)](./LICENSE-CONTENT.md)

STM32F103RC 模拟水分仪 Modbus 从机，EBYTE NT1-B 完成 UART 与以太网透明转发，MThings 作为上位机主站，构成一条完整的工业通信实验链路。

![系统总体架构](./assets/system-architecture.png)

## 项目基本信息

| 项目 | 内容 |
|---|---|
| 项目时间 | **2026 年 4 月**（终稿日期为 2026-04-30） |
| 项目组长 | **胡荣杰** |
| 实际开发者 | **胡荣杰——唯一实际设计与开发者** |
| 贡献说明 | 课程小组中如有其他关联姓名，均为挂名；固件、协议实现、网络配置、硬件联调、故障排查、实验验证、PPT 与实验报告均由 **胡荣杰一人完成**。 |
| 项目类型 | 嵌入式固件 + 工业协议 + 网络集成实验 |
| 原型成本 | **现有资料未记录，待后续补充** |
| 完成状态 | 已完成 FC03/FC06、网络桥接、上位机监控和读写验证 |

## 商业化与应用分析

该原型展示了传统串口仪表低成本联网改造方案：保留 RTU 设备固件，在前端增加串口转以太网透明桥。相同模式可用于水分仪、称重仪表、环境传感器、实验装置和需要接入局域网监控的老旧工业设备。

若要产品化，需将模拟数据替换为经过标定的真实传感器采集，并增加隔离 RS-485、浪涌/ESD 防护、看门狗恢复、标准 Modbus 异常响应、配置安全、设备部署和长期可靠性测试。当前成果是已验证的通信原型，不是可直接用于生产计量的水分仪。

## 系统架构与协议

```text
MThings 主站
    │ TCP/IP
路由器 / 局域网
    │ TCP，端口 502
EBYTE NT1-B（透明转发）
    │ UART，9600 bps，8N1
STM32F103RC Modbus RTU 从机，地址 0x01
```

本实验采用 **Modbus RTU over TCP**，不是原生 Modbus TCP。NT1-B 只按字节透明转发，不转换协议；如果 MThings 选择 Modbus TCP，会增加 7 字节 MBAP 头，STM32 收到的首字节变为 `0x00` 而不是从机地址 `0x01`，因此丢弃报文。改用 RTU over TCP 后完成了全链路通信。

### 已实现寄存器表

| 地址 | 寄存器 | 权限 | 初始/原始值 | 上位机显示 |
|---|---|---|---:|---:|
| `0x0010` | 砂石种类 | 读写 | `1` | `1` |
| `0x0011` | 模拟实时水分值 | 只读 | `128` | `12.8%` |
| `0x0012` | 模拟温度值 | 只读 | `253` | `25.3 ℃` |

水分和温度按实际值 ×10 存储，并通过有界伪随机游走每秒更新。它们是**模拟信号**，不是物理水分传感器的测量值。

### 固件逻辑

- USART2 以 `9600 bps, 8N1` 中断接收 RTU 报文；
- 主循环在收到数据后等待 20 ms，校验从机地址和 CRC16-Modbus，再分发功能码；
- `0x03` 读取保持寄存器，`0x06` 写单个允许写入的寄存器并回显请求；
- 水分、温度寄存器拒绝 FC06 写入；
- USART1 以 `115200 bps` 输出启动状态、模拟值、收发帧和寄存器操作。

## 软件架构与数据流

工程采用简单的前后台结构。后台 `USART2_IRQHandler()` 每收到一个字节就写入 64 字节接收缓冲区；前台主循环每秒更新一次模拟传感器，并在接收缓冲区非空后调用 `Modbus_Process()`。

```text
USART2 接收中断
    ↓ 将字节写入 g_rx_buf[]
主循环等待 20 ms 判断帧结束
    ↓
Modbus_Process()
    ├─ 最小长度检查
    ├─ 从机地址检查
    ├─ CRC16-Modbus 校验
    ├─ Handle_FC03() → 组装寄存器读取应答
    └─ Handle_FC06() → 权限检查、写入并回显请求
                         ↓
                    USART2 返回应答

Sensor_Simulate() → g_regs[] ← MThings 读写请求
                         ↓
                   USART1 调试日志
```

| 源码区域 | 作用 |
|---|---|
| `USER/main.c` | 寄存器表、CRC、USART2 驱动、接收中断、FC03/FC06、数据模拟和主循环 |
| `SYSTEM/usart/` | USART1 控制台及 `printf` 重定向 |
| `SYSTEM/delay/`、`SYSTEM/sys/` | SysTick 延时和 STM32 系统辅助函数 |
| `STM32F10x_FWLIB/`、`CORE/` | 标准外设库、CMSIS 与启动支持 |

实验版把协议逻辑集中在一个源文件中，便于按执行顺序学习。生产版本应把传输层、Modbus 解析器、寄存器模型和传感器采集拆成可单独测试的模块。

![上位机实验结果](./assets/experiment-results.png)

## 已验证结果

- MThings 经以太网/UART 完整链路成功读取三个寄存器；
- 向 `0x0010` 写入 `3` 后收到 FC06 规定的原帧回显，并可再次读回；
- 向只读 `0x0011` 写入时固件拒绝响应，上位机超时；
- 模拟水分与温度曲线可持续刷新；
- USART1 能输出十六进制请求与应答帧。

## 下载、编译与复现

### 1. 下载源码

```bash
git clone https://github.com/WuWingKit/-stm32-moisture-meter-modbus.git
cd -- -stm32-moisture-meter-modbus
```

仓库名称以连字符开头，因此 `cd` 命令中的 `--` 用于避免把目录名解析为参数。没有安装 Git 时，可在 GitHub 选择 **Code → Download ZIP** 并解压。

### 2. 编译与烧录

1. 安装 Keil MDK 5、与现有工程兼容的 ARM Compiler 以及 ST-Link 驱动；
2. 打开 `USER/Moisture_Meter.uvprojx`，执行 **Build**（`F7`）；
3. 通过 SWD 连接 STM32F103RC，执行 **Download**（`F8`）；
4. 将 TTL-USB 接到 USART1，以 `115200 bps` 检查启动日志。

### 3. 连接通信链路

1. STM32 `PA2 (TX)` 接 NT1-B `RXD`，`PA3 (RX)` 接 `TXD`；
2. 两块板必须共地，供电前应根据 NT1-B 自身规格书确认其电压；
3. 将 NT1-B 设置为 TCP Server，串口参数为 `9600 bps, 8N1`，网络地址必须能从上位机局域网访问。

![STM32 与 NT1-B 接线](./assets/hardware-wiring.png)

### 4. 配置与测试 MThings

1. 选择 **Modbus RTU over TCP**，不要选择 Modbus TCP；
2. 填写 NT1-B IP/端口及从机地址 `1`；
3. 添加保持寄存器地址 16、17、18，水分和温度设置系数 `0.1`；
4. 开始轮询，随后修改地址 16 并再次读取验证。

实验报告中的原始地址是 `192.168.0.10:502`；复现实验时应按自己的网络选择无冲突地址。

## 仓库内容

- `USER/Moisture_Meter.uvprojx`：Keil MDK 工程入口
- `USER/main.c`：协议、寄存器、传感器模拟与双串口逻辑
- `SYSTEM/`、`CORE/`、`STM32F10x_FWLIB/`：平台支持、启动文件与 STM32 库
- `docs/experiment-report.docx`：新增的完整原始实验报告
- `presentation/project-presentation.pptx`：原始 44 页实验汇报
- `assets/`：从汇报中导出的 README 配图

## 当前限制与扩展方向

- 尚未连接真实水分传感器；
- 报文边界采用固定 20 ms 等待，而不是定时器/空闲线状态机；
- 非法写入直接丢弃，没有返回标准 Modbus 异常帧；
- 尚未实现 FC16、原生 Modbus TCP、RS-485 多从机、鉴权和云端遥测。

## 协议

项目原创实验报告、汇报和图片采用 **CC BY-NC-SA 4.0**；详见 [LICENSE-CONTENT.md](./LICENSE-CONTENT.md)。源码及第三方厂商组件仍遵循各自文件中注明的许可条款。

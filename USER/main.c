#include "stm32f10x.h"
#include "delay.h"
#include "usart.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>

/* ============================================================
 * 水分仪 Modbus 从机模拟
 * USART1: PA9=TX, PA10=RX, 115200bps  (调试口，printf输出)
 * USART2: PA2=TX, PA3=RX, 9600bps     (Modbus通信口)
 * 从机地址: 0x01
 * ============================================================ */

#define SLAVE_ADDR   0x01    /* 从机地址，与协议手册一致 */
#define UART_BAUD    9600

/* ---------- 寄存器定义 ---------- */
#define REG_ADDR_SETTING   0x0000  /* 水分仪地址设置 */
#define REG_MOISTURE_PARAM 0x0001  /* 水分参数设置   */
#define REG_STONE_TYPE     0x0010  /* 砂石种类       */
#define REG_MOISTURE       0x0011  /* 实时水分值     */
#define REG_TEMPERATURE    0x0012  /* 温度值         */

/* ---------- 寄存器存储区 ----------
 * 用数组模拟寄存器，下标=地址
 * 这里只实现协议要求的关键寄存器
 * --------------------------------- */
static int16_t g_regs[0x20];   /* 0x0000~0x001F 共32个寄存器 */

/* ---------- 串口收发缓冲 ---------- */
#define RX_BUF_LEN  64
static uint8_t g_rx_buf[RX_BUF_LEN];
static uint8_t g_rx_cnt = 0;

/* ============================================================
 * CRC16-Modbus 计算
 * 参数: buf=数据指针, len=字节数
 * 返回: 16位CRC值（低字节是要先发的那个）
 * ============================================================ */
uint16_t CRC16_Modbus(uint8_t *buf, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    uint16_t i, j;
    for(i = 0; i < len; i++)
    {
        crc ^= buf[i];
        for(j = 0; j < 8; j++)
        {
            if(crc & 0x0001)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }
    return crc;  /* 低字节=crc&0xFF 先发，高字节=crc>>8 后发 */
}

/* ============================================================
 * USART2 初始化 (Modbus通信口)
 * ============================================================ */
void UART2_Init(void)
{
    GPIO_InitTypeDef  GPIO_IS;
    USART_InitTypeDef USART_IS;
    NVIC_InitTypeDef  NVIC_IS;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    /* PA2 TX */
    GPIO_IS.GPIO_Pin   = GPIO_Pin_2;
    GPIO_IS.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_IS.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_IS);

    /* PA3 RX */
    GPIO_IS.GPIO_Pin  = GPIO_Pin_3;
    GPIO_IS.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_IS);

    USART_IS.USART_BaudRate            = UART_BAUD;
    USART_IS.USART_WordLength          = USART_WordLength_8b;
    USART_IS.USART_StopBits            = USART_StopBits_1;
    USART_IS.USART_Parity              = USART_Parity_No;
    USART_IS.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_IS.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART2, &USART_IS);

    NVIC_IS.NVIC_IRQChannel                   = USART2_IRQn;
    NVIC_IS.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_IS.NVIC_IRQChannelSubPriority        = 0;
    NVIC_IS.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_IS);

    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
    USART_Cmd(USART2, ENABLE);
}

/* 发送一个字节 */
static void UART2_SendByte(uint8_t d)
{
    while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
    USART_SendData(USART2, d);
}

/* 发送多字节 */
static void UART2_SendBuf(uint8_t *buf, uint8_t len)
{
    uint8_t i;
    for(i = 0; i < len; i++) UART2_SendByte(buf[i]);
}

/* ============================================================
 * USART2 接收中断
 * ============================================================ */
void USART2_IRQHandler(void)
{
    if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        uint8_t d = (uint8_t)USART_ReceiveData(USART2);
        if(g_rx_cnt < RX_BUF_LEN)
            g_rx_buf[g_rx_cnt++] = d;
        USART_ClearITPendingBit(USART2, USART_IT_RXNE);
    }
}

/* ============================================================
 * 打印十六进制数据（调试用）
 * ============================================================ */
static void Print_HexFrame(uint8_t *buf, uint8_t len, const char *tag)
{
    uint8_t i;
    printf("[%s] %d bytes: ", tag, len);
    for(i = 0; i < len; i++)
        printf("%02X ", buf[i]);
    printf("\r\n");
}

/* ============================================================
 * 处理功能码 03：读寄存器
 *
 * 请求帧: [ADDR][03][REG_H][REG_L][CNT_H][CNT_L][CRC_L][CRC_H]
 * 应答帧: [ADDR][03][字节数][数据...][CRC_L][CRC_H]
 * ============================================================ */
static void Handle_FC03(uint8_t *req)
{
    uint16_t reg_addr = ((uint16_t)req[2] << 8) | req[3];
    uint16_t reg_cnt  = ((uint16_t)req[4] << 8) | req[5];
    uint8_t  resp[64];
    uint8_t  idx = 0;
    uint16_t i, val, crc;

    printf("FC03: read reg 0x%04X, count %d\r\n", reg_addr, reg_cnt);

    /* 检查地址范围 */
    if(reg_addr + reg_cnt > 0x20)
    {
        printf("FC03: addr out of range!\r\n");
        return;
    }

    /* 构建应答帧头 */
    resp[idx++] = SLAVE_ADDR;
    resp[idx++] = 0x03;
    resp[idx++] = (uint8_t)(reg_cnt * 2);

    /* 填入寄存器值（每个寄存器2字节，高字节在前） */
    for(i = 0; i < reg_cnt; i++)
    {
        val = (uint16_t)g_regs[reg_addr + i];
        resp[idx++] = (uint8_t)(val >> 8);
        resp[idx++] = (uint8_t)(val & 0xFF);
    }

    /* 计算并追加CRC（低字节先） */
    crc = CRC16_Modbus(resp, idx);
    resp[idx++] = (uint8_t)(crc & 0xFF);
    resp[idx++] = (uint8_t)(crc >> 8);

    Print_HexFrame(resp, idx, "TX");
    UART2_SendBuf(resp, idx);
}

/* ============================================================
 * 处理功能码 06：写单个寄存器
 *
 * 请求帧: [ADDR][06][REG_H][REG_L][VAL_H][VAL_L][CRC_L][CRC_H]
 * 应答帧: 与请求帧完全相同（回显）
 * ============================================================ */
static void Handle_FC06(uint8_t *req)
{
    uint16_t reg_addr = ((uint16_t)req[2] << 8) | req[3];
    int16_t  val      = (int16_t)(((uint16_t)req[4] << 8) | req[5]);

    printf("FC06: write reg 0x%04X = %d\r\n", reg_addr, val);

    /* 检查是否是只读寄存器，只读寄存器拒绝写入 */
    if(reg_addr == REG_MOISTURE || reg_addr == REG_TEMPERATURE)
    {
        printf("FC06: reg 0x%04X is read-only!\r\n", reg_addr);
        return;
    }

    if(reg_addr >= 0x20)
    {
        printf("FC06: addr out of range!\r\n");
        return;
    }

    /* 更新寄存器 */
    g_regs[reg_addr] = val;

    /* 回显：应答帧与请求帧完全相同（8字节，含CRC） */
    Print_HexFrame(req, 8, "TX");
    UART2_SendBuf(req, 8);
}

/* ============================================================
 * Modbus 帧处理主函数
 * ============================================================ */
static void Modbus_Process(void)
{
    uint16_t crc_recv, crc_calc;
    uint8_t  fc;

    /* 最短帧：功能码03/06 的请求帧都是8字节 */
    if(g_rx_cnt < 8) return;

    Print_HexFrame(g_rx_buf, g_rx_cnt, "RX");

    /* 检查从机地址 */
    if(g_rx_buf[0] != SLAVE_ADDR)
    {
        printf("addr mismatch: 0x%02X (expect 0x%02X)\r\n", g_rx_buf[0], SLAVE_ADDR);
        goto clear;
    }

    /* 验证CRC（帧的最后2字节：低字节在前） */
    crc_recv = (uint16_t)g_rx_buf[g_rx_cnt-2]
             | ((uint16_t)g_rx_buf[g_rx_cnt-1] << 8);
    crc_calc = CRC16_Modbus(g_rx_buf, g_rx_cnt - 2);
    if(crc_recv != crc_calc)
    {
        printf("CRC error: recv=%04X calc=%04X\r\n", crc_recv, crc_calc);
        goto clear;
    }

    /* 根据功能码分发处理 */
    fc = g_rx_buf[1];
    if(fc == 0x03) Handle_FC03(g_rx_buf);
    else if(fc == 0x06) Handle_FC06(g_rx_buf);
    else printf("unsupported FC: 0x%02X\r\n", fc);

clear:
    g_rx_cnt = 0;
}

/* ============================================================
 * 简易伪随机数（线性同余，无需标准库rand）
 * ============================================================ */
static uint32_t g_rand_seed = 12345;
static uint16_t Rand_Next(void)
{
    g_rand_seed = g_rand_seed * 1103515245 + 12345;
    return (uint16_t)(g_rand_seed >> 16);
}

/* ============================================================
 * 初始化寄存器默认值
 * ============================================================ */
static void Regs_Init(void)
{
    g_regs[REG_ADDR_SETTING] = 1;     /* 水分仪地址默认1     */
    g_regs[REG_STONE_TYPE]   = 1;     /* 默认1号砂石（粗砂） */
    g_regs[REG_MOISTURE]     = 128;   /* 模拟水分12.8%       */
    g_regs[REG_TEMPERATURE]  = 253;   /* 模拟温度25.3℃      */
}

/* ============================================================
 * 模拟传感器数据实时变化
 * 水分值: 50~200  (5.0%~20.0%)   每次变化 ±1~±5
 * 温度值: 150~350 (15.0℃~35.0℃) 每次变化 ±1~±3
 * ============================================================ */
static void Sensor_Simulate(void)
{
    static uint16_t tick = 0;
    int16_t delta;
    int16_t val;

    tick++;
    if(tick < 100) return;   /* 100 * 10ms = 1秒 */
    tick = 0;

    /* --- 水分值变化 --- */
    delta = (int16_t)(Rand_Next() % 11) - 5;   /* -5 ~ +5 */
    val = g_regs[REG_MOISTURE] + delta;
    if(val < 50)  val = 50;     /* 下限 5.0% */
    if(val > 200) val = 200;    /* 上限 20.0% */
    g_regs[REG_MOISTURE] = val;

    /* --- 温度值变化 --- */
    delta = (int16_t)(Rand_Next() % 7) - 3;    /* -3 ~ +3 */
    val = g_regs[REG_TEMPERATURE] + delta;
    if(val < 150) val = 150;    /* 下限 15.0℃ */
    if(val > 350) val = 350;    /* 上限 35.0℃ */
    g_regs[REG_TEMPERATURE] = val;

    printf("[SENSOR] Moisture=%d.%d%%  Temp=%d.%dC\r\n",
           g_regs[REG_MOISTURE] / 10, g_regs[REG_MOISTURE] % 10,
           g_regs[REG_TEMPERATURE] / 10, g_regs[REG_TEMPERATURE] % 10);
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void)
{
    SystemInit();
    delay_init();
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    uart_init(115200);   /* USART1: 调试口，printf输出到串口助手 */
    UART2_Init();        /* USART2: Modbus通信口 */
    Regs_Init();         /* 初始化寄存器默认值 */

    printf("\r\n=== Moisture Meter Modbus Slave ===\r\n");
    printf("Slave Addr: 0x%02X\r\n", SLAVE_ADDR);
    printf("USART2: %d bps (Modbus)\r\n", UART_BAUD);
    printf("USART1: 115200 bps (Debug)\r\n");
    printf("Waiting for Modbus requests...\r\n\r\n");

    while(1)
    {
        delay_ms(10);
        Sensor_Simulate();      /* 每秒更新一次模拟数据 */

        if(g_rx_cnt > 0)
        {
            delay_ms(20);       /* 再等20ms确保整帧都收到 */
            Modbus_Process();   /* 处理这帧数据 */
        }
    }
}

#include "sys.h"
#include "usart.h"	  
////////////////////////////////////////////////////////////////////////////////// 	 
//??????ucos,??????????????????.
#if SYSTEM_SUPPORT_OS
#include "includes.h"					//ucos ???	  
#endif

 

//////////////////////////////////////////////////////////////////
//???????????,???printf????,??????????use MicroLIB	  
#if 1
#pragma import(__use_no_semihosting)             
//????????????????                 
struct __FILE 
{ 
	int handle; 

}; 

FILE __stdout;       
//????_sys_exit()???????????????    
void _sys_exit(int x) 
{ 
	x = x; 
} 
//?????fputc???? 
int fputc(int ch, FILE *f)
{      
	while((USART1->SR&0X40)==0);//???????,??????????   
    USART1->DR = (u8) ch;      
	return ch;
}
#endif 

 
 
#if EN_USART1_RX   //???????????
//????1?§Ø???????
//???,???USARTx->SR????????????????   	
u8 USART_RX_BUF[USART_REC_LEN];     //???????,???USART_REC_LEN?????.
//??????
//bit15??	?????????
//bit14??	?????0x0d
//bit13~0??	?????????§¹??????
u16 USART_RX_STA=0;       //?????????	  
  
void uart_init(u32 bound){
  //GPIO???????
  GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	 
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1|RCC_APB2Periph_GPIOA, ENABLE);	
  
	//USART1_TX   GPIOA.9
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9; 
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;	
  GPIO_Init(GPIOA, &GPIO_InitStructure);
   
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;//PA10
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=3 ;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;		
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			
	NVIC_Init(&NVIC_InitStructure);	
  

	USART_InitStructure.USART_BaudRate = bound;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;	

  USART_Init(USART1, &USART_InitStructure); 
  USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
  USART_Cmd(USART1, ENABLE);                    

}
/*
Name:void UsartSendData(USART_TypeDef *USARTx,uint8_t u8Data)
----------------------
Des:  
Ref:
Paras: 
Author: zx
Date:   
*/
void UsartSendData(USART_TypeDef *USARTx,uint8_t u8Data)
{		
	while((USART1->SR&0X40)==0);
	USART1 -> DR = u8Data;
}
/*
void Usart_SendBuf(USART_TypeDef *USARTx,uint8_t *u8pBuf,uint16_t u16Len)
----------------------
Des:  ??????????????
Ref:
Paras: 
Author:zx
Date:2024??04??08??
*/
void UsartSendBuf(USART_TypeDef *USARTx,uint8_t *u8pBuf,uint16_t u16Len)
{
    uint8_t u8LoopData = 0;	  
    if(USARTx == USART1)
	{	  
        for(u8LoopData = 0; u8LoopData < u16Len; u8LoopData++)   
        {
            UsartSendData(USARTx,u8pBuf[u8LoopData]);
        }
	}
}

void USART1_IRQHandler(void)                	
	{
	u8 Res;
#if SYSTEM_SUPPORT_OS 		
	OSIntEnter();    
#endif
	if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)  
		{
		Res =USART_ReceiveData(USART1);	
		
		if((USART_RX_STA&0x8000)==0)
			{
			if(USART_RX_STA&0x4000)
				{
				if(Res!=0x0a)USART_RX_STA=0;
				else USART_RX_STA|=0x8000;	
				}
			else 
				{	
				if(Res==0x0d)USART_RX_STA|=0x4000;
				else
					{
					USART_RX_BUF[USART_RX_STA&0X3FFF]=Res ;
					USART_RX_STA++;
					if(USART_RX_STA>(USART_REC_LEN-1))USART_RX_STA=0;
					}		 
				}
			}   		 
     } 
#if SYSTEM_SUPPORT_OS 	
	OSIntExit();  											 
#endif
} 
#endif	


/**
  ******************************************************************************
  * @file    FMC_SDRAM/stm32f4xx_it.c 
  * @author  MCD Application Team
  * @version V1.0.1
  * @date    11-November-2013
  * @brief   Main Interrupt Service Routines.
  *         This file provides template for all exceptions handler and
  *         peripherals interrupt service routine.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2013 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_it.h"
#include "bsp_usart_debug.h"
#include "bsp_usart_extra.h"
#include "bsp_usart_gps.h"
#include "bsp_mpu6050_exti.h"
#include "bsp_general_tim2.h"
#include "bsp_general_tim3.h"
#include "bsp_general_tim4.h"
#include "bsp_general_tim5.h"
#include "bsp_basic_tim7.h"
#include "bsp_basic_tim6.h"
#include "bsp_gpio_led.h"
#include "multi_button.h"
#include "inv_mpu.h"
#include "inv_mpu_dmp_motion_driver.h" 
#include "bsp_mpu6050.h"
#include "bsp_fsmc_lcd.h"
#include <string.h>
#include "common.h"
#include "bsp_i2c_touch.h"
#include "platform_nrf.h"
#include "bsp_adc1_independent_dual.h"
#include "bsp_adc3_independent_dual.h"
#include "malloc.h"
#include "menu.h"
#include "bsp_gpio_stick.h"
#include "nmea_decode_test.h"

extern void TimingDelay_Decrement(void);
extern void TimeStamp_Increment(void);

unsigned int Task_Delay[5];

unsigned int button_hearttick;

extern volatile uint16_t ADC1_Value[NUM_OF_ADC1CHANNEL];
extern volatile uint16_t ADC3_Value[NUM_OF_ADC3CHANNEL];

extern uint8_t txbuf[32];
extern uint8_t rxbuf[32];

uint8_t ADC_Value1_High, ADC_Value1_Low;
uint8_t ADC_Value2_High, ADC_Value2_Low;  
uint8_t ADC_Value3_High, ADC_Value3_Low;
uint8_t ADC_Value4_High, ADC_Value4_Low; 
uint8_t ADC_Value5_High, ADC_Value5_Low;
uint8_t ADC_Value6_High, ADC_Value6_Low; 
uint8_t ADC_Value7_High, ADC_Value7_Low;
uint8_t ADC_Value8_High, ADC_Value8_Low; 
uint8_t ADC_Value9_High, ADC_Value9_Low;
uint8_t ADC_Value10_High,ADC_Value10_Low;

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/******************************************************************************/
/*            Cortex-M4 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
  * @brief  This function handles NMI exception.
  * @param  None
  * @retval None
  */
void NMI_Handler(void)
{
}

/**
  * @brief  This function handles Hard Fault exception.
  * @param  None
  * @retval None
  */
void HardFault_Handler(void)
{
  /* Go to infinite loop when Hard Fault exception occurs */
  printf("Hard Fault\r\n");
  while (1)
  {}
}

/**
  * @brief  This function handles Memory Manage exception.
  * @param  None
  * @retval None
  */
void MemManage_Handler(void)
{
  /* Go to infinite loop when Memory Manage exception occurs */
  while (1)
  {}
}

/**
  * @brief  This function handles Bus Fault exception.
  * @param  None
  * @retval None
  */
void BusFault_Handler(void)
{
  /* Go to infinite loop when Bus Fault exception occurs */
  while (1)
  {}
}

/**
  * @brief  This function handles Usage Fault exception.
  * @param  None
  * @retval None
  */
void UsageFault_Handler(void)
{
  /* Go to infinite loop when Usage Fault exception occurs */
  while (1)
  {}
}

/**
  * @brief  This function handles Debug Monitor exception.
  * @param  None
  * @retval None
  */
void DebugMon_Handler(void)
{}

/**
  * @brief  This function handles SVCall exception.
  * @param  None
  * @retval None
  */
void SVC_Handler(void)
{}

/**
  * @brief  This function handles PendSV_Handler exception.
  * @param  None
  * @retval None
  */
void PendSV_Handler(void)
{}

/**
  * @brief  This function handles SysTick Handler.
  * @param  None
  * @retval None
  */
void SysTick_Handler(void)
{
	unsigned char i;
  TimingDelay_Decrement();
	//TimeStamp_Increment();
	
	for(i=0;i<5;i++)
	{
		if(Task_Delay[i])
		{
      Task_Delay[i]--;
		}
	}
}

/******************************************************************************/
/*                 STM32F4xx Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f429_439xx.s).                         */
/******************************************************************************/
void DEBUG_USART_IRQHandler(void)
{
    uint8_t ucTemp;
	if(USART_GetITStatus(DEBUG_USART,USART_IT_RXNE)!=RESET)
	{		
		  ucTemp = USART_ReceiveData( DEBUG_USART );
      USART_SendData(DEBUG_USART,ucTemp);    
	}	 
}

void EXPAND_USART_IRQHandler(void)
{
    uint8_t ucTemp;
	if(USART_GetITStatus(EXPAND_USART,USART_IT_RXNE)!=RESET)
	{		
		  ucTemp = USART_ReceiveData( EXPAND_USART );
      USART_SendData(EXPAND_USART,ucTemp);    
	}	 
}

void MPU_IRQHandler(void)
{
	if(EXTI_GetITStatus(MPU_INT_EXTI_LINE) != RESET) //
	{
    LED1_TOGGLE;
    EXTI_ClearITPendingBit(MPU_INT_EXTI_LINE); //
  }  
}

//定时器2中断服务函数：
void GENERAL_TIM2_IRQHandler(void)
{
	if(TIM_GetITStatus(GENERAL_TIM2,TIM_IT_Update) != RESET ) 
	{
		    ADC_Value1_High  = (ADC1_Value[0]>>8)&0xFF;
        ADC_Value1_Low   =  ADC1_Value[0]&0xFF;
        ADC_Value2_High  = (ADC1_Value[1]>>8)&0xFF;
        ADC_Value2_Low   =  ADC1_Value[1]&0xFF;
		    ADC_Value3_High  = (ADC1_Value[2]>>8)&0xFF;
        ADC_Value3_Low   =  ADC1_Value[2]&0xFF;
        ADC_Value4_High  = (ADC1_Value[3]>>8)&0xFF;
        ADC_Value4_Low   =  ADC1_Value[3]&0xFF;
		    ADC_Value5_High  = (ADC1_Value[4]>>8)&0xFF;
        ADC_Value5_Low   =  ADC1_Value[4]&0xFF;
        ADC_Value6_High  = (ADC1_Value[5]>>8)&0xFF;
        ADC_Value6_Low   =  ADC1_Value[5]&0xFF;
		    ADC_Value7_High  = (ADC1_Value[6]>>8)&0xFF;
        ADC_Value7_Low   =  ADC1_Value[6]&0xFF;
        ADC_Value8_High  = (ADC3_Value[0]>>8)&0xFF;
        ADC_Value8_Low   =  ADC3_Value[0]&0xFF;
		    ADC_Value9_High  = (ADC3_Value[1]>>8)&0xFF;
        ADC_Value9_Low   =  ADC3_Value[1]&0xFF;
        ADC_Value10_High = (ADC3_Value[2]>>8)&0xFF;
        ADC_Value10_Low  =  ADC3_Value[2]&0xFF;

        txbuf[0]  = ADC_Value1_High;
        txbuf[1]  = ADC_Value1_Low;
        txbuf[2]  = ADC_Value2_High;
        txbuf[3]  = ADC_Value2_Low;
        txbuf[4]  = ADC_Value3_High;
        txbuf[5]  = ADC_Value3_Low;			
		    txbuf[6]  = ADC_Value4_High;
        txbuf[7]  = ADC_Value4_Low;
        txbuf[8]  = ADC_Value5_High;
        txbuf[9]  = ADC_Value5_Low;
        txbuf[10] = ADC_Value6_High;
        txbuf[11] = ADC_Value6_Low;
		    txbuf[12] = ADC_Value7_High;
        txbuf[13] = ADC_Value7_Low;
		    txbuf[14] = ADC_Value8_High;
        txbuf[15] = ADC_Value8_Low;
        txbuf[16] = ADC_Value9_High;
        txbuf[17] = ADC_Value9_Low;
		    txbuf[18] = ADC_Value10_High;
        txbuf[19] = ADC_Value10_Low;
		
		    //nrf24l01_send();
        
        TIM_ClearITPendingBit(GENERAL_TIM2,TIM_IT_Update);  		 
	}		 	
}

//定时器3中断服务函数：
void GENERAL_TIM3_IRQHandler(void)
{
	if(TIM_GetITStatus(GENERAL_TIM3,TIM_IT_Update) != RESET ) 
	{	
		// printf("stick1 %d\n\r",STICK_Scan(STICK1_GPIO_PORT,STICK1_PIN));
		// printf("stick2 %d\n\r",STICK_Scan(STICK2_GPIO_PORT,STICK2_PIN));
		// printf("stick3 %d\n\r",STICK_Scan(STICK3_GPIO_PORT,STICK3_PIN));
			
//		printf("ADC1_Value:%d\r\n ", ADC1_Value[0]);
//		printf("ADC2_Value:%d\r\n ", ADC1_Value[1]);
//		printf("ADC3_Value:%d\r\n ", ADC1_Value[2]);
//		printf("ADC4_Value:%d\r\n ", ADC1_Value[3]);
//		printf("ADC5_Value:%d\r\n ", ADC1_Value[4]);
//		printf("ADC6_Value:%d\r\n ", ADC1_Value[5]);
//		printf("ADC7_Value:%d\r\n ", ADC1_Value[6]);
//		printf("ADC8_Value:%d\r\n ", ADC3_Value[0]);
//		printf("ADC9_Value:%d\r\n ", ADC3_Value[1]);
//		printf("ADC10_Value:%d\r\n", ADC3_Value[2]);
//		printf("\r\n");

      nmea_decode_test();
  }
  TIM_ClearITPendingBit(GENERAL_TIM3,TIM_IT_Update);  		  	
}

//定时器4中断服务函数：
void TIM4_IRQHandler(void)
{
    if(TIM_GetITStatus(TIM4,TIM_IT_Update)==SET)
    {       
		
		
    }
    TIM_ClearITPendingBit(TIM4,TIM_IT_Update); 
}

//定时器5中断服务函数：
void GENERAL_TIM5_IRQHandler(void)
{
	if (TIM_GetITStatus(GENERAL_TIM5, TIM_IT_Update) != RESET) //检查指定的TIM中断发生与否:TIM 中断源 
    {		
		if(++button_hearttick >= 8) 
		{
			button_ticks();
			button_hearttick = 0;
		}
		
		TIM_ClearITPendingBit(GENERAL_TIM5, TIM_IT_Update);//清除TIMx的中断待处理位:TIM 中断源 
	}
}

//定时器6中断服务函数：
void BASIC_TIM_IRQHandler(void)
{
	if(TIM_GetITStatus( BASIC_TIM, TIM_IT_Update) != RESET ) 
	{	
		
		//RTC_TimeAndDate_Show(); //显示时间和日期
    menu_button_set();
		TIM_ClearITPendingBit(BASIC_TIM , TIM_IT_Update);  		 
	}		 	
}


//定时器7中断服务函数：
void GENERAL_TIM7_IRQHandler(void)
{
	if(TIM_GetITStatus(BASIC_TIM7, TIM_IT_Update) != RESET ) 
	{
		  
      TIM_ClearITPendingBit(BASIC_TIM7,TIM_IT_Update);  		 
	}		 	
}

//GPS DMA中断服务函数：
void GPS_DMA_IRQHANDLER(void)
{
    if(DMA_GetITStatus(GPS_USART_DMA_STREAM,GPS_DMA_IT_HT) )         /* DMA 半传输完成 */
    {
		GPS_HalfTransferEnd = 1;                //设置半传输完成标志位
		DMA_ClearITPendingBit (GPS_USART_DMA_STREAM,GPS_DMA_IT_HT); 
    }

    else if(DMA_GetITStatus(GPS_USART_DMA_STREAM,GPS_DMA_IT_TC))     /* DMA 传输完成 */
    {
		GPS_TransferEnd = 1;                    //设置传输完成标志位
		DMA_ClearITPendingBit(GPS_USART_DMA_STREAM,GPS_DMA_IT_TC);
    }
}

//触摸
void GTP_IRQHandler(void)
{
	if(EXTI_GetITStatus(GTP_INT_EXTI_LINE) != RESET) //确保是否产生了EXTI Line中断
	{
		GTP_TouchProcess();    
		EXTI_ClearITPendingBit(GTP_INT_EXTI_LINE);   //清除中断标志位
	}  
}

/**
  * @}
  */ 

/**
  * @}
  */ 

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

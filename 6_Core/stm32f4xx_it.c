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
#include "gt9xx.h"
#include "platform_nrf.h"
#include "bsp_adc1_independent_dual.h"
#include "bsp_adc3_independent_dual.h"
#include "menu.h"
#include "bsp_gpio_stick.h"
#include "nmea_decode_test.h"

extern void TimeStamp_Increment(void);

extern volatile uint16_t ADC1_Value[NUM_OF_ADC1CHANNEL];
extern volatile uint16_t ADC3_Value[NUM_OF_ADC3CHANNEL];

extern uint8_t txbuf[32];
extern uint8_t rxbuf[32];

extern volatile uint8_t finish_1hz,finish_2hz,finish_5hz,finish_10hz,finish_20hz,finish_33hz,finish_50hz,finish_100hz;
extern volatile uint8_t finish_button_10ms;

volatile uint8_t ADC_Value1_High, ADC_Value1_Low;
volatile uint8_t ADC_Value2_High, ADC_Value2_Low;
volatile uint8_t ADC_Value3_High, ADC_Value3_Low;
volatile uint8_t ADC_Value4_High, ADC_Value4_Low;
volatile uint8_t ADC_Value5_High, ADC_Value5_Low;
volatile uint8_t ADC_Value6_High, ADC_Value6_Low;
volatile uint8_t ADC_Value7_High, ADC_Value7_Low;
volatile uint8_t ADC_Value8_High, ADC_Value8_Low;
volatile uint8_t ADC_Value9_High, ADC_Value9_Low;
volatile uint8_t ADC_Value10_High,ADC_Value10_Low;

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
  TimeStamp_Increment();
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
  if (USART_GetITStatus(DEBUG_USART, USART_IT_RXNE) != RESET)
  {
    ucTemp = USART_ReceiveData(DEBUG_USART);
    USART_SendData(DEBUG_USART, ucTemp);
  }
}

void EXPAND_USART_IRQHandler(void)
{
  uint8_t ucTemp;
  if (USART_GetITStatus(EXPAND_USART, USART_IT_RXNE) != RESET)
  {
    ucTemp = USART_ReceiveData(EXPAND_USART);
    USART_SendData(EXPAND_USART, ucTemp);
  }
}

void EXTI9_5_IRQHandler(void)
{
  if (EXTI_GetITStatus(MPU_INT_EXTI_LINE) != RESET)
  {
    EXTI_ClearITPendingBit(MPU_INT_EXTI_LINE);
  }
}

void EXTI15_10_IRQHandler(void)
{
  if (EXTI_GetITStatus(GTP_INT_EXTI_LINE) != RESET)
  {
    GTP_NotifyInterrupt();
    EXTI_ClearITPendingBit(GTP_INT_EXTI_LINE);
  }
}

// 定时器2中断服务函数：
void GENERAL_TIM2_IRQHandler(void)
{
  if (TIM_GetITStatus(GENERAL_TIM2, TIM_IT_Update) != RESET)
  {
    uint8_t channel;
    uint16_t sample;
    /* Read each DMA value once before splitting it into bytes. */
    for(channel = 0U; channel < 10U; channel++) {
      sample = channel < NUM_OF_ADC1CHANNEL ? ADC1_Value[channel] :
               ADC3_Value[channel - NUM_OF_ADC1CHANNEL];
      txbuf[channel * 2U] = (uint8_t)(sample >> 8);
      txbuf[channel * 2U + 1U] = (uint8_t)sample;
    }

    // nrf24l01_send();

    TIM_ClearITPendingBit(GENERAL_TIM2, TIM_IT_Update);
  }
}

// 定时器3中断服务函数：
void GENERAL_TIM3_IRQHandler(void)
{
  if (TIM_GetITStatus(GENERAL_TIM3, TIM_IT_Update) != RESET)
  {

  }
  TIM_ClearITPendingBit(GENERAL_TIM3, TIM_IT_Update);
}

// 定时器4中断服务函数：
void TIM4_IRQHandler(void)
{
  if (TIM_GetITStatus(TIM4, TIM_IT_Update) == SET)
  {
  }
  TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
}

// 定时器5中断服务函数：
void GENERAL_TIM5_IRQHandler(void)
{
  static uint16_t tim5_count;
  if(TIM_GetITStatus(GENERAL_TIM5, TIM_IT_Update) != RESET) {
    /* TIM5 is configured for a 1 ms interrupt. */
    tim5_count++;
    if(tim5_count % 10U == 0U) finish_100hz = 1U;
    if(tim5_count % 20U == 0U) finish_50hz = 1U;
    if(tim5_count % 50U == 0U) finish_20hz = 1U;
    if(tim5_count % 100U == 0U) finish_10hz = 1U;
    if(tim5_count % 200U == 0U) finish_5hz = 1U;
    if(tim5_count % 500U == 0U) finish_2hz = 1U;
    if(tim5_count == 1000U) {
      tim5_count = 0U;
      finish_1hz = 1U;
    }
    TIM_ClearITPendingBit(GENERAL_TIM5, TIM_IT_Update);
  }
}

// 定时器6中断服务函数：
void BASIC_TIM_IRQHandler(void)
{
  if (TIM_GetITStatus(BASIC_TIM, TIM_IT_Update) != RESET)
  {
    GTP_Tick10ms();
    finish_button_10ms = 1;
    TIM_ClearITPendingBit(BASIC_TIM, TIM_IT_Update);
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
    GPS_DMA_ReceiveIRQ();
}

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

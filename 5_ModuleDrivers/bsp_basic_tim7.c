#include "bsp_basic_tim7.h"

/**
  * @brief  ͨ�ö�ʱ�� TIMx,x[1,8]�ж����ȼ�����
  * @param  ��
  * @retval ��
  */
static void GENERAL_TIM7_NVIC_Configuration(void)
{
    NVIC_InitTypeDef NVIC_InitStructure; 
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);	// �����ж���Ϊ0		
    NVIC_InitStructure.NVIC_IRQChannel = GENERAL_TIM7_IRQn;// �����ж���Դ 		
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;// ������ռ���ȼ�	   
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;// ���������ȼ�	
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

/*
 * ע�⣺TIM_TimeBaseInitTypeDef�ṹ��������5����Ա��TIM6��TIM7�ļĴ�������ֻ��
 * TIM_Prescaler��TIM_Period������ʹ��TIM6��TIM7��ʱ��ֻ���ʼ����������Ա���ɣ�
 * ����������Ա��ͨ�ö�ʱ���͸߼���ʱ������.
 *-----------------------------------------------------------------------------
 * TIM_Prescaler         ����
 * TIM_CounterMode	     TIMx,x[6,7]û�У��������У�������ʱ����
 * TIM_Period            ����
 * TIM_ClockDivision     TIMx,x[6,7]û�У���������(������ʱ��)
 * TIM_RepetitionCounter TIMx,x[1,8]����(�߼���ʱ��)
 *-----------------------------------------------------------------------------
 */
static void GENERAL_TIM7_Mode_Config(uint16_t arr,uint16_t psc)
{
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;

	// ����TIMx_CLK,x[6,7] 
    RCC_APB1PeriphClockCmd(GENERAL_TIM7_CLK, ENABLE); 

    /* �ۼ� TIM_Period�������һ�����»����ж�*/		
    //����ʱ����0������4999����Ϊ5000�Σ�Ϊһ����ʱ����
    TIM_TimeBaseStructure.TIM_Period = arr;    //10000-1;       
	
    // ͨ�ÿ��ƶ�ʱ��ʱ��ԴTIMxCLK = HCLK/2=84MHz 
    // �趨��ʱ��Ƶ��Ϊ=TIMxCLK/(TIM_Prescaler+1)=10000Hz
    TIM_TimeBaseStructure.TIM_Prescaler = psc;  //8400-1;	

    // ��ʼ����ʱ��TIMx, x[1,8]
    TIM_TimeBaseInit(GENERAL_TIM7, &TIM_TimeBaseStructure);
    
    // �����ʱ�������жϱ�־λ
    TIM_ClearFlag(GENERAL_TIM7, TIM_FLAG_Update);

    // ������ʱ�������ж�
    TIM_ITConfig(GENERAL_TIM7,TIM_IT_Update,ENABLE);

    // ʹ�ܶ�ʱ��
    TIM_Cmd(GENERAL_TIM7, ENABLE);	
}

/**
  * @brief  ��ʼ����ʱ����ʱ��1ms����һ���ж�
  * @param  ��
  * @retval ��
  */
void GENERAL_TIM7_InitConfiguration(uint16_t arr,uint16_t psc)
{
    GENERAL_TIM7_Mode_Config(arr,psc);
	GENERAL_TIM7_NVIC_Configuration();	
}




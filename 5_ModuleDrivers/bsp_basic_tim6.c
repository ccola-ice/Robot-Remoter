#include "bsp_basic_tim6.h"

 /**
  * @brief  ������ʱ�� TIMx,x[6,7]�ж����ȼ�����
  * @param  ��
  * @retval ��
  */
static void BASIC_TIM6_NVIC_Config(void)
{
    NVIC_InitTypeDef NVIC_InitStructure; 
    // �����ж���Ϊ0
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);		
    // �����ж���Դ
    NVIC_InitStructure.NVIC_IRQChannel = BASIC_TIM_IRQn; 	
    // ������ռ���ȼ�
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;	 
    // ���������ȼ�
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;	
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

/*
 * ע�⣺TIM_TimeBaseInitTypeDef�ṹ��������5����Ա��TIM6��TIM7�ļĴ�������ֻ��
 * TIM_Prescaler��TIM_Period������ʹ��TIM6��TIM7��ʱ��ֻ���ʼ����������Ա���ɣ�
 * ����������Ա��ͨ�ö�ʱ���͸߼���ʱ������.
 *-----------------------------------------------------------------------------
 * TIM_Prescaler            ����
 * TIM_CounterMode	        TIMx,x[6,7]û�У��������У�������ʱ����
 * TIM_Period               ����
 * TIM_ClockDivision        TIMx,x[6,7]û�У���������(������ʱ��)
 * TIM_RepetitionCounter    TIMx,x[1,8]����(�߼���ʱ��)
 *-----------------------------------------------------------------------------
 */
static void BASIC_TIM6_Mode_Config(uint16_t arr,uint16_t psc)
{
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;

    // ����TIMx_CLK,x[6,7] 
    RCC_APB1PeriphClockCmd(BASIC_TIM_CLK, ENABLE); 

    /* �ۼ� TIM_Period�������һ�����»����ж�*/		
    //����ʱ����0������4999����Ϊ5000�Σ�Ϊһ����ʱ����
    TIM_TimeBaseStructure.TIM_Period = arr;//5000-1;       

    //��ʱ��ʱ��ԴTIMxCLK = 2 * PCLK1  
    //				PCLK1 = HCLK / 4 
    //				=> TIMxCLK=HCLK/2=SystemCoreClock/2=84MHz
    // �趨��ʱ��Ƶ��Ϊ=TIMxCLK/(TIM_Prescaler+1)=10000Hz
    TIM_TimeBaseStructure.TIM_Prescaler = psc;//8400-1;	

    // ��ʼ����ʱ��TIMx, x[2,3,4,5]
    TIM_TimeBaseInit(BASIC_TIM, &TIM_TimeBaseStructure);

    // �����ʱ�������жϱ�־λ
    TIM_ClearFlag(BASIC_TIM, TIM_FLAG_Update);

    // ������ʱ�������ж�
    TIM_ITConfig(BASIC_TIM,TIM_IT_Update,ENABLE);

    // ʹ�ܶ�ʱ��
    TIM_Cmd(BASIC_TIM, ENABLE);	
}

/**
  * @brief  ��ʼ��������ʱ����ʱ
  * @param  ��
  * @retval ��
  */
void BASIC_TIM6_Configuration(uint16_t arr,uint16_t psc)
{
	BASIC_TIM6_NVIC_Config();	
    BASIC_TIM6_Mode_Config(arr,psc);
}



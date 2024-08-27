#include "bsp_general_tim4.h"

static void GENERAL_TIM4_NVIC_Config(void)
{
	NVIC_InitTypeDef NVIC_InitStructure;	

    NVIC_InitStructure.NVIC_IRQChannel=GENERAL_TIM4_IRQn; 
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=0; 
    NVIC_InitStructure.NVIC_IRQChannelSubPriority=3; 
    NVIC_InitStructure.NVIC_IRQChannelCmd=ENABLE;
    NVIC_Init(&NVIC_InitStructure);	
}

static void GENERAL_TIM4_Mode_Config(uint16_t arr,uint16_t psc)
{
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
    TIM_ICInitTypeDef TIM_ICInitStructure;   
    
    //PD12  CH1  A��, PD13 CH2  B��
    GENERAL_TIM4_CLK_CMD(GENERAL_TIM4_CLK, ENABLE); //ʹ��TIM4ʱ��
    
    TIM_DeInit(GENERAL_TIM4);
    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.TIM_Period = arr; //65535;  //�趨��������װֵ   TIMx_ARR = 1024*4-1 ����360�ߵ�
    TIM_TimeBaseStructure.TIM_Prescaler = psc; //0; //TIM4ʱ��Ԥ��Ƶֵ
    TIM_TimeBaseStructure.TIM_ClockDivision =TIM_CKD_DIV1 ;//����ʱ�ӷָ� T_dts = T_ck_int    
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; //TIM���ϼ��� 
    TIM_TimeBaseInit(GENERAL_TIM4, &TIM_TimeBaseStructure);              

    TIM_EncoderInterfaceConfig(GENERAL_TIM4, TIM_EncoderMode_TI12, TIM_ICPolarity_BothEdge ,TIM_ICPolarity_BothEdge);

    TIM_ICStructInit(&TIM_ICInitStructure);//���ṹ���е�����ȱʡ����
    TIM_ICInitStructure.TIM_ICFilter = 0;  //ѡ������Ƚ��˲��� 
    TIM_ICInit(GENERAL_TIM4, &TIM_ICInitStructure);//��TIM_ICInitStructure�е�ָ��������ʼ��TIM4

    //����ж�����
    TIM_ITConfig(GENERAL_TIM4,TIM_IT_Update,ENABLE); //����TIM4����ж�
    TIM_ClearFlag(GENERAL_TIM4,TIM_FLAG_Update);
    TIM_SetCounter(GENERAL_TIM4,0x7fff); //TIM4->CNT=0
    TIM_Cmd(GENERAL_TIM4, ENABLE); 
}

void GENERAL_TIM4_InitConfiguration(uint16_t arr,uint16_t psc)
{
    GENERAL_TIM4_NVIC_Config();
    GENERAL_TIM4_Mode_Config(arr,psc);
}



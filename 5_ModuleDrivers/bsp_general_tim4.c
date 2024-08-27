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
    
    //PD12  CH1  A相, PD13 CH2  B相
    GENERAL_TIM4_CLK_CMD(GENERAL_TIM4_CLK, ENABLE); //使能TIM4时钟
    
    TIM_DeInit(GENERAL_TIM4);
    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.TIM_Period = arr; //65535;  //设定计数器重装值   TIMx_ARR = 1024*4-1 这是360线的
    TIM_TimeBaseStructure.TIM_Prescaler = psc; //0; //TIM4时钟预分频值
    TIM_TimeBaseStructure.TIM_ClockDivision =TIM_CKD_DIV1 ;//设置时钟分割 T_dts = T_ck_int    
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; //TIM向上计数 
    TIM_TimeBaseInit(GENERAL_TIM4, &TIM_TimeBaseStructure);              

    TIM_EncoderInterfaceConfig(GENERAL_TIM4, TIM_EncoderMode_TI12, TIM_ICPolarity_BothEdge ,TIM_ICPolarity_BothEdge);

    TIM_ICStructInit(&TIM_ICInitStructure);//将结构体中的内容缺省输入
    TIM_ICInitStructure.TIM_ICFilter = 0;  //选择输入比较滤波器 
    TIM_ICInit(GENERAL_TIM4, &TIM_ICInitStructure);//将TIM_ICInitStructure中的指定参数初始化TIM4

    //溢出中断设置
    TIM_ITConfig(GENERAL_TIM4,TIM_IT_Update,ENABLE); //允许TIM4溢出中断
    TIM_ClearFlag(GENERAL_TIM4,TIM_FLAG_Update);
    TIM_SetCounter(GENERAL_TIM4,0x7fff); //TIM4->CNT=0
    TIM_Cmd(GENERAL_TIM4, ENABLE); 
}

void GENERAL_TIM4_InitConfiguration(uint16_t arr,uint16_t psc)
{
    GENERAL_TIM4_NVIC_Config();
    GENERAL_TIM4_Mode_Config(arr,psc);
}



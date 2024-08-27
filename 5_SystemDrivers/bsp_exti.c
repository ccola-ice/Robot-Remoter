#include "bsp_exti.h"
#include "bsp_gpio_led.h"     

void ExtiGpioInit(void);
void ExtiNvicInit(void);
void ExtiModeInit(void);

volatile uint8_t key_flag  = 0;
volatile uint8_t menu_flag = 0;

void Exti_Init(void)
{
//	ExtiGpioInit();
//	ExtiNvicInit();
//	ExtiModeInit();
}
void ExtiGpioInit(void)
{
}

//void EXTI1_IRQHandler (void)
//{
//	if(EXTI_GetITStatus(EXTI_Line1) == SET )
//	{

//		//--------------------------------
//		EXTI_ClearFlag(EXTI_Line1);
//	}
//}

//void EXTI2_IRQHandler (void)
//{
//	if(EXTI_GetITStatus(EXTI_Line2) == SET )
//	{
//        
//		//--------------------------------
//		EXTI_ClearFlag(EXTI_Line2);
//	}
//}

//void EXTI3_IRQHandler (void)
//{
//	if(EXTI_GetITStatus(EXTI_Line3) == SET )
//	{
//		//

//		//--------------------------------
//		EXTI_ClearFlag(EXTI_Line3);
//	}
//}

//void EXTI4_IRQHandler (void)
//{
//	if(EXTI_GetITStatus(EXTI_Line4) == SET )
//	{
//		//
//        
//		//--------------------------------
//		EXTI_ClearFlag(EXTI_Line4);
//	}
//}



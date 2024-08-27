#include "platform_mpu.h"
#include "bsp_general_tim3.h"
#include "bsp_rtc.h"
#include "bsp_usart_debug.h"
#include "inv_mpu.h"
#include "inv_mpu_dmp_motion_driver.h" 
#include "bsp_mpu6050.h"
#include "LCD_test.h"
#include "bsp_gpio_led.h"

////MPU6050检查
//void mpu6050_check(void)
//{
//    //检测MPU6050
//	if (MPU6050ReadID() == 1)
//	{
//        printf("\r\nMPU6050检测成功！\r\n");    
//	}
//	else
//	{
//        printf("\r\nMPU6050检测失败！\r\n"); 
//	}
//}

////MPU6050数据获取
//void mpu6050_get(void)
//{
//    MPU6050ReadAcc(Acel);
//    printf("加速度：%8d%8d%8d ",Acel[0],Acel[1],Acel[2]);
//    MPU6050ReadGyro(Gyro);
//    printf("陀螺仪%8d%8d%8d ",Gyro[0],Gyro[1],Gyro[2]);
//    MPU6050_ReturnTemp(&Temp);
//    printf("温度%8.2f\r\n",Temp);				
//    
//    #if 0	
//    {
//        char cStr [ 70 ];
//        sprintf ( cStr, "Acc  :%6d%6d%6d",Acel[0],Acel[1],Acel[2] );	//加速度原始数据


//        ILI9806G_DispStringLine_EN(LINE(7),cStr);			

//        sprintf ( cStr, "Gyro :%6d%6d%6d",Gyro[0],Gyro[1],Gyro[2] );	//角原始数据

//        ILI9806G_DispStringLine_EN(LINE(8),cStr);			

//        sprintf ( cStr, "Tem  :%6.2f",Temp );							//温度值
//        ILI9806G_DispStringLine_EN(LINE(9),cStr);			
//    }
//    #endif
//    
//}







#include "platform_mpu.h"
#include "bsp_general_tim3.h"
#include "bsp_rtc.h"
#include "bsp_usart_debug.h"
#include "inv_mpu.h"
#include "inv_mpu_dmp_motion_driver.h" 
#include "bsp_mpu6050.h"
#include "LCD_test.h"
#include "bsp_gpio_led.h"

////MPU6050���
//void mpu6050_check(void)
//{
//    //���MPU6050
//	if (MPU6050ReadID() == 1)
//	{
//        printf("\r\nMPU6050���ɹ���\r\n");    
//	}
//	else
//	{
//        printf("\r\nMPU6050���ʧ�ܣ�\r\n"); 
//	}
//}

////MPU6050���ݻ�ȡ
//void mpu6050_get(void)
//{
//    MPU6050ReadAcc(Acel);
//    printf("���ٶȣ�%8d%8d%8d ",Acel[0],Acel[1],Acel[2]);
//    MPU6050ReadGyro(Gyro);
//    printf("������%8d%8d%8d ",Gyro[0],Gyro[1],Gyro[2]);
//    MPU6050_ReturnTemp(&Temp);
//    printf("�¶�%8.2f\r\n",Temp);				
//    
//    #if 0	
//    {
//        char cStr [ 70 ];
//        sprintf ( cStr, "Acc  :%6d%6d%6d",Acel[0],Acel[1],Acel[2] );	//���ٶ�ԭʼ����


//        ILI9806G_DispStringLine_EN(LINE(7),cStr);			

//        sprintf ( cStr, "Gyro :%6d%6d%6d",Gyro[0],Gyro[1],Gyro[2] );	//��ԭʼ����

//        ILI9806G_DispStringLine_EN(LINE(8),cStr);			

//        sprintf ( cStr, "Tem  :%6.2f",Temp );							//�¶�ֵ
//        ILI9806G_DispStringLine_EN(LINE(9),cStr);			
//    }
//    #endif
//    
//}







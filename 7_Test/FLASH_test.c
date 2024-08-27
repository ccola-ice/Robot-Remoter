#include "FLASH_test.h"

uint8_t flash_read_buf[1024];    //���붨��Ϊȫ�ֱ���(ջ�ռ�Ĭ��ֵΪ0x00000400)
uint8_t flash_write_buf[1024];   //���붨��Ϊȫ�ֱ���(ջ�ռ�Ĭ��ֵΪ0x00000400)
//uint8_t flash_pic_buf[4096];     //���붨��Ϊȫ�ֱ���(ջ�ռ�Ĭ��ֵΪ0x00000400)

//FLASH��д����
void flash_test(void)
{
    uint8_t flash_device_id;
	uint32_t flash_id;
    uint16_t i;
    
    printf("\r\n==========================W25Q128��д���Կ�ʼ=================================\r\n");

    flash_device_id = FLASH_Read_DeviceID();
	flash_id = FLASH_Read_FlashID();
    printf("\r\nThe Flash Device ID is 0x%02x\r\n",flash_device_id);
    printf("\r\nThe Flash ID is 0x%x\r\n",flash_id);
	
    printf("\r\n������0��������ʼ\r\n");
    FLASH_Erase_Sectors(0*4096);
    printf("\r\n������0����������\r\n");
    printf("\r\n������1��������ʼ\r\n");
    FLASH_Erase_Sectors(1*4096);
    printf("\r\n������1����������\r\n");
    printf("\r\n������2��������ʼ\r\n");
    FLASH_Erase_Sectors(2*4096);
    printf("\r\n������2����������\r\n");
    
    printf("\r\nд�뿪ʼ\r\n");
    for(i=0;i<1024;i++)
    {
        flash_write_buf[i] = 0x77;
    }
    FLASH_Write_Page_v3(flash_write_buf,0x00000A,1024);
    printf("\r\nд�����\r\n");
    
    printf("\r\n��ȡ��ʼ\r\n");
    FLASH_Read_Data(flash_read_buf,0x00000A,1024);
    for(i=0;i<1024;i++)
    {
        printf("0x%02x ",flash_read_buf[i]);     
    }
    printf("\r\n��ȡ����\r\n");
    
    printf("\r\n==========================W25Q128��д���Խ���=================================\r\n");

//    //ͼƬ���ݶ�д����
//    FLASH_Page_Progarm_v3(0X1000,gImage_1,3200);
//    FLASH_Read_Data(0X1000,flash_pic_buf,4096);
//    for(i=0;i<4096;i++)
//    {
//        printf("0x%02x ",flash_pic_buf[i]);     
//    }
}



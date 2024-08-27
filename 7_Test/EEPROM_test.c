#include "EEPROM_test.h"

//EEPROM��д����
void eeprom_test(void)
{
    uint8_t receive_data;
    uint8_t send_buffer[8];
    uint8_t receive_buffer[8];
    uint8_t send_buffer_2[256];
    uint8_t receive_buffer_2[256];
    uint8_t send_buffer_3[100];
    uint8_t receive_buffer_3[100];
    uint16_t i;
    
    for(i=0;i<8;i++)
    {
        send_buffer[i] = i;
    }
    for(i=0;i<256;i++)
    {
        send_buffer_2[i] = i;
    }
    for(i=0;i<100;i++)
    {
        send_buffer_3[i] = i;
    }
    
    printf("\r\n==========================AT24C08��д���Կ�ʼ=================================\r\n");
    
    printf("\r\n\r\n���ֽڶ�д���Կ�ʼ");
    EEPROM_Byte_Write(8,0xEE);
    EEPROM_Random_Read(8,&receive_data);
    printf("\r\n���ֽڶ�д���Խ�����Received Data:\r\n");
    printf("0x%02x",receive_data);
    
    printf("\r\n\r\nҳ��д���Կ�ʼ");
    EEPROM_Page_Write(0,send_buffer,8);
    EEPROM_Sequential_Read(0,receive_buffer,8);
    printf("\r\nҳ��д���Խ�����Received Data:\r\n");
    for(i=0; i<8; i++)
    {
        printf("0x%02x ",receive_buffer[i]);
    }
    
    printf("\r\n\r\n˳���д����V1��ʼ");
    EEPROM_Sequential_Write(0,send_buffer_2,256);
    EEPROM_Sequential_Read(0,receive_buffer_2,256);
    printf("\r\n˳���д����V1������Received Data:\r\n");
    for(i=0; i<256; i++)
    {
        printf("0x%02x ",receive_buffer_2[i]);
    }

    printf("\r\n\r\n˳���д����V2��ʼ");
    EEPROM_Sequential_Write_Update(13,send_buffer_3,50);
    EEPROM_Sequential_Read(0,receive_buffer_3,70);
    printf("\r\n˳���д����V2������Received Data:\r\n");
    for(i=0; i<70; i++)
    {
        printf("0x%02x ",receive_buffer_3[i]);
    }

    printf("\r\n==========================AT24C08��д���Խ���=================================\r\n");
}







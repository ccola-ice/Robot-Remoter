#include "EEPROM_test.h"

//EEPROM∂¡–¥≤‚ ‘
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
    
    printf("\r\n==========================AT24C08∂¡–¥≤‚ ‘ø™ º=================================\r\n");
    
    printf("\r\n\r\nµ•◊÷Ω⁄∂¡–¥≤‚ ‘ø™ º");
    EEPROM_Byte_Write(8,0xEE);
    EEPROM_Random_Read(8,&receive_data);
    printf("\r\nµ•◊÷Ω⁄∂¡–¥≤‚ ‘Ω· ¯£¨Received Data:\r\n");
    printf("0x%02x",receive_data);
    
    printf("\r\n\r\n“≥∂¡–¥≤‚ ‘ø™ º");
    EEPROM_Page_Write(0,send_buffer,8);
    EEPROM_Sequential_Read(0,receive_buffer,8);
    printf("\r\n“≥∂¡–¥≤‚ ‘Ω· ¯£¨Received Data:\r\n");
    for(i=0; i<8; i++)
    {
        printf("0x%02x ",receive_buffer[i]);
    }
    
    printf("\r\n\r\nÀ≥–Ú∂¡–¥≤‚ ‘V1ø™ º");
    EEPROM_Sequential_Write(0,send_buffer_2,256);
    EEPROM_Sequential_Read(0,receive_buffer_2,256);
    printf("\r\nÀ≥–Ú∂¡–¥≤‚ ‘V1Ω· ¯£¨Received Data:\r\n");
    for(i=0; i<256; i++)
    {
        printf("0x%02x ",receive_buffer_2[i]);
    }

    printf("\r\n\r\nÀ≥–Ú∂¡–¥≤‚ ‘V2ø™ º");
    EEPROM_Sequential_Write_Update(13,send_buffer_3,50);
    EEPROM_Sequential_Read(0,receive_buffer_3,70);
    printf("\r\nÀ≥–Ú∂¡–¥≤‚ ‘V2Ω· ¯£¨Received Data:\r\n");
    for(i=0; i<70; i++)
    {
        printf("0x%02x ",receive_buffer_3[i]);
    }

    printf("\r\n==========================AT24C08∂¡–¥≤‚ ‘Ω· ¯=================================\r\n");
}







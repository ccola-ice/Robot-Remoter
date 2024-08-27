#ifndef __FONT_H
#define __FONT_H       
#include "stm32f4xx.h"
#include "fonts.h"

#define LINE(x)  ((x) * (((sFONT *)LCD_GetFont())->Height))
#define LINEY(x) ((x) * (((sFONT *)LCD_GetFont())->Width))

/** @defgroup FONTS_Exported_Types
  * @{
  */ 
typedef struct _tFont
{    
  const uint8_t *table;
  uint16_t Width;
  uint16_t Height;
  
} sFONT;


extern sFONT Font24x48;
extern sFONT Font16x32;
extern sFONT Font8x16;

#define GBKCODE_START_ADDRESS   1254*4096

/*��ȡ�ֿ�ĺ���*/
//�����ȡ�����ַ���ģ����ĺ�������ucBufferΪ�����ģ��������usCharΪ�����ַ��������룩
#define GetGBKCode( ucBuffer, usChar )  GetGBKCode_from_EXFlash(ucBuffer,usChar)   //Ҫ֧��������Ҫʵ�ֱ�����
int GetGBKCode_from_EXFlash( uint8_t * pBuffer, uint16_t c);


//�����ַ���С
#define      WIDTH_CH_CHAR		                32	    	//�����ַ���� 
#define      HEIGHT_CH_CHAR		              	32		  	//�����ַ��߶� 





#endif /* __FONT_H */










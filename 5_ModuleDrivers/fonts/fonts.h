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

/*获取字库的函数*/
//定义获取中文字符字模数组的函数名，ucBuffer为存放字模数组名，usChar为中文字符（国标码）
#define GetGBKCode( ucBuffer, usChar )  GetGBKCode_from_EXFlash(ucBuffer,usChar)   //要支持中文需要实现本函数
int GetGBKCode_from_EXFlash( uint8_t * pBuffer, uint16_t c);


//中文字符大小
#define      WIDTH_CH_CHAR		                32	    	//中文字符宽度 
#define      HEIGHT_CH_CHAR		              	32		  	//中文字符高度 





#endif /* __FONT_H */










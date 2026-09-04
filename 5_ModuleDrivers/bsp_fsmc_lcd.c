#include "bsp_fsmc_lcd.h"
#include "bsp_SysTick.h"
#include "fonts.h"	

//根据液晶扫描方向而变化的XY像素宽度
//调用ILI9806G_GramScan函数设置方向时会自动更改
uint16_t LCD_X_LENGTH = ILI9806G_MORE_PIXEL;
uint16_t LCD_Y_LENGTH = ILI9806G_LESS_PIXEL;

//液晶屏扫描模式，本变量主要用于方便选择触摸屏的计算参数
//参数可选值为0-7
//调用ILI9806G_GramScan函数设置方向时会自动更改
//LCD刚初始化完成时会使用本默认值
uint8_t LCD_SCAN_MODE = 5;

static sFONT *LCD_Currentfonts = &Font16x32;  	//英文字体
static uint16_t CurrentTextColor   = WHITE;		//前景色
static uint16_t CurrentBackColor   = BLACK;		//背景色

//缓存读取回来的字模数据
static uint8_t ucBuffer [ WIDTH_CH_CHAR*HEIGHT_CH_CHAR/8 ];	

__inline void ILI9806G_Write_Cmd ( uint16_t usCmd );
__inline void ILI9806G_Write_Data ( uint16_t usData );
__inline uint16_t ILI9806G_Read_Data ( void );
static void                 ILI9806G_Write_Cmd           ( uint16_t usCmd );
static void                 ILI9806G_Write_Data          ( uint16_t usData );
static uint16_t             ILI9806G_Read_Data           ( void );
static void                	ILI9806G_GPIO_Config         ( void );
static void                	ILI9806G_FSMC_Config         ( void );
static void                	ILI9806G_REG_Config          ( void );
static void                	ILI9806G_SetCursor           ( uint16_t usX, uint16_t usY );
static __inline void       	ILI9806G_FillColor           ( uint32_t ulAmout_Point, uint16_t usColor );
static uint16_t            	ILI9806G_Read_PixelData      ( void );

/**
  * @brief  用于 ILI9806G 简单延时函数
  * @param  nCount ：延时计数值
  * @retval 无
  */	


///**
//  * @brief  向ILI9806G写入命令
//  * @param  usCmd :要写入的命令（表寄存器地址）
//  * @retval 无
//  */	
//static void ILI9806G_Write_Cmd ( uint16_t usCmd )
//{
//	* ( volatile uint16_t * ) ( FSMC_Addr_ILI9806G_CMD ) = usCmd;
//}

///**
//  * @brief  向ILI9806G写入数据
//  * @param  usData :要写入的数据
//  * @retval 无
//  */	
//static void ILI9806G_Write_Data ( uint16_t usData )
//{
//	* ( volatile uint16_t * ) ( FSMC_Addr_ILI9806G_DATA ) = usData;
//}

///**
//  * @brief  从ILI9806G读取数据
//  * @param  无
//  * @retval 读取到的数据
//  */	
//static uint16_t ILI9806G_Read_Data ( void )
//{
//	return ( * ( volatile uint16_t * ) ( FSMC_Addr_ILI9806G_DATA ) );	
//}

/**
  * @brief  初始化ILI9806G的IO引脚
  * @param  无
  * @retval 无
  */
static void ILI9806G_GPIO_Config ( void )
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/* 使能FSMC对应相应管脚时钟*/
	RCC_AHB1PeriphClockCmd (
													/*控制信号*/
													ILI9806G_CS_CLK|ILI9806G_DC_CLK|ILI9806G_WR_CLK|
													ILI9806G_RD_CLK	|ILI9806G_BK_CLK|ILI9806G_RST_CLK|
													/*数据信号*/
													ILI9806G_D0_CLK|ILI9806G_D1_CLK|	ILI9806G_D2_CLK | 
													ILI9806G_D3_CLK | ILI9806G_D4_CLK|ILI9806G_D5_CLK|
													ILI9806G_D6_CLK | ILI9806G_D7_CLK|ILI9806G_D8_CLK|
													ILI9806G_D9_CLK | ILI9806G_D10_CLK|ILI9806G_D11_CLK|
													ILI9806G_D12_CLK | ILI9806G_D13_CLK|ILI9806G_D14_CLK|
													ILI9806G_D15_CLK	, ENABLE );
		
	
	/* 配置FSMC相对应的数据线,FSMC-D0~D15 */	
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;

    GPIO_InitStructure.GPIO_Pin = ILI9806G_D0_PIN; 
    GPIO_Init(ILI9806G_D0_PORT, &GPIO_InitStructure);
    GPIO_PinAFConfig(ILI9806G_D0_PORT,ILI9806G_D0_PinSource,FSMC_AF);

    GPIO_InitStructure.GPIO_Pin = ILI9806G_D1_PIN; 
    GPIO_Init(ILI9806G_D1_PORT, &GPIO_InitStructure);
    GPIO_PinAFConfig(ILI9806G_D1_PORT,ILI9806G_D1_PinSource,FSMC_AF);

    GPIO_InitStructure.GPIO_Pin = ILI9806G_D2_PIN; 
    GPIO_Init(ILI9806G_D2_PORT, &GPIO_InitStructure);
    GPIO_PinAFConfig(ILI9806G_D2_PORT,ILI9806G_D2_PinSource,FSMC_AF);

    GPIO_InitStructure.GPIO_Pin = ILI9806G_D3_PIN; 
    GPIO_Init(ILI9806G_D3_PORT, &GPIO_InitStructure);
    GPIO_PinAFConfig(ILI9806G_D3_PORT,ILI9806G_D3_PinSource,FSMC_AF);

    GPIO_InitStructure.GPIO_Pin = ILI9806G_D4_PIN; 
    GPIO_Init(ILI9806G_D4_PORT, &GPIO_InitStructure);
    GPIO_PinAFConfig(ILI9806G_D4_PORT,ILI9806G_D4_PinSource,FSMC_AF);

    GPIO_InitStructure.GPIO_Pin = ILI9806G_D5_PIN; 
    GPIO_Init(ILI9806G_D5_PORT, &GPIO_InitStructure);
    GPIO_PinAFConfig(ILI9806G_D5_PORT,ILI9806G_D5_PinSource,FSMC_AF);

    GPIO_InitStructure.GPIO_Pin = ILI9806G_D6_PIN; 
    GPIO_Init(ILI9806G_D6_PORT, &GPIO_InitStructure);
    GPIO_PinAFConfig(ILI9806G_D6_PORT,ILI9806G_D6_PinSource,FSMC_AF);

    GPIO_InitStructure.GPIO_Pin = ILI9806G_D7_PIN; 
    GPIO_Init(ILI9806G_D7_PORT, &GPIO_InitStructure);
    GPIO_PinAFConfig(ILI9806G_D7_PORT,ILI9806G_D7_PinSource,FSMC_AF);

    GPIO_InitStructure.GPIO_Pin = ILI9806G_D8_PIN; 
    GPIO_Init(ILI9806G_D8_PORT, &GPIO_InitStructure);
    GPIO_PinAFConfig(ILI9806G_D8_PORT,ILI9806G_D8_PinSource,FSMC_AF);

    GPIO_InitStructure.GPIO_Pin = ILI9806G_D9_PIN; 
    GPIO_Init(ILI9806G_D9_PORT, &GPIO_InitStructure);
    GPIO_PinAFConfig(ILI9806G_D9_PORT,ILI9806G_D9_PinSource,FSMC_AF);

    GPIO_InitStructure.GPIO_Pin = ILI9806G_D10_PIN; 
    GPIO_Init(ILI9806G_D10_PORT, &GPIO_InitStructure);
    GPIO_PinAFConfig(ILI9806G_D10_PORT,ILI9806G_D10_PinSource,FSMC_AF);

    GPIO_InitStructure.GPIO_Pin = ILI9806G_D11_PIN; 
    GPIO_Init(ILI9806G_D11_PORT, &GPIO_InitStructure);
    GPIO_PinAFConfig(ILI9806G_D11_PORT,ILI9806G_D11_PinSource,FSMC_AF);

    GPIO_InitStructure.GPIO_Pin = ILI9806G_D12_PIN; 
    GPIO_Init(ILI9806G_D12_PORT, &GPIO_InitStructure);
    GPIO_PinAFConfig(ILI9806G_D12_PORT,ILI9806G_D12_PinSource,FSMC_AF);

    GPIO_InitStructure.GPIO_Pin = ILI9806G_D13_PIN; 
    GPIO_Init(ILI9806G_D13_PORT, &GPIO_InitStructure);
    GPIO_PinAFConfig(ILI9806G_D13_PORT,ILI9806G_D13_PinSource,FSMC_AF);

    GPIO_InitStructure.GPIO_Pin = ILI9806G_D14_PIN; 
    GPIO_Init(ILI9806G_D14_PORT, &GPIO_InitStructure);
    GPIO_PinAFConfig(ILI9806G_D14_PORT,ILI9806G_D14_PinSource,FSMC_AF);

    GPIO_InitStructure.GPIO_Pin = ILI9806G_D15_PIN; 
    GPIO_Init(ILI9806G_D15_PORT, &GPIO_InitStructure);
    GPIO_PinAFConfig(ILI9806G_D15_PORT,ILI9806G_D15_PinSource,FSMC_AF);

	/* 配置FSMC相对应的控制线
	 * FSMC_NOE   :LCD-RD
	 * FSMC_NWE   :LCD-WR
	 * FSMC_NE1   :LCD-CS
	 * FSMC_A0    :LCD-DC
	 */
    GPIO_InitStructure.GPIO_Pin = ILI9806G_RD_PIN; 
    GPIO_Init(ILI9806G_RD_PORT, &GPIO_InitStructure);
    GPIO_PinAFConfig(ILI9806G_RD_PORT,ILI9806G_RD_PinSource,FSMC_AF);

    GPIO_InitStructure.GPIO_Pin = ILI9806G_WR_PIN; 
    GPIO_Init(ILI9806G_WR_PORT, &GPIO_InitStructure);
    GPIO_PinAFConfig(ILI9806G_WR_PORT,ILI9806G_WR_PinSource,FSMC_AF);

    GPIO_InitStructure.GPIO_Pin = ILI9806G_CS_PIN; 
    GPIO_Init(ILI9806G_CS_PORT, &GPIO_InitStructure);   
    GPIO_PinAFConfig(ILI9806G_CS_PORT,ILI9806G_CS_PinSource,FSMC_AF);  

    GPIO_InitStructure.GPIO_Pin = ILI9806G_DC_PIN; 
    GPIO_Init(ILI9806G_DC_PORT, &GPIO_InitStructure);
    GPIO_PinAFConfig(ILI9806G_DC_PORT,ILI9806G_DC_PinSource,FSMC_AF);
	
	/* 配置LCD复位RST控制管脚*/
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_InitStructure.GPIO_Pin = ILI9806G_RST_PIN; 
	GPIO_Init ( ILI9806G_RST_PORT, & GPIO_InitStructure );
		
	/* 配置LCD背光控制管脚BLK*/
	GPIO_InitStructure.GPIO_Pin = ILI9806G_BK_PIN; 
	GPIO_Init ( ILI9806G_BK_PORT, & GPIO_InitStructure );
}


 /**
  * @brief  LCD  FSMC 模式配置
  * @param  无
  * @retval 无
  */
static void ILI9806G_FSMC_Config ( void )
{
	FSMC_NORSRAMInitTypeDef  FSMC_NORSRAMInitStructure;
	FSMC_NORSRAMTimingInitTypeDef  readWriteTiming; 	
		
	RCC_AHB3PeriphClockCmd(RCC_AHB3Periph_FSMC,ENABLE);/* 使能FSMC时钟*/

	//地址建立时间（ADDSET）为1个HCLK 5/168M=30ns
	readWriteTiming.FSMC_AddressSetupTime      = 0x04;	 //地址建立时间
	//数据保持时间（DATAST）+ 1个HCLK = 12/168M=72ns	
	readWriteTiming.FSMC_DataSetupTime         = 0x0f;	 //数据建立时间
	//选择控制的模式
	readWriteTiming.FSMC_AccessMode            = FSMC_AccessMode_B;	//模式B,异步NOR FLASH模式，与ILI9806G的8080时序匹配
	
	/*以下配置与模式B无关*/	
	readWriteTiming.FSMC_AddressHoldTime       = 0x00;//地址保持时间//地址保持时间（ADDHLD）模式A未用到	
	readWriteTiming.FSMC_BusTurnAroundDuration = 0x00;//设置总线转换周期，仅用于复用模式的NOR操作	
	readWriteTiming.FSMC_CLKDivision           = 0x00;//设置时钟分频，仅用于同步类型的存储器	
	readWriteTiming.FSMC_DataLatency           = 0x00;//数据保持时间，仅用于同步型的NOR

	FSMC_NORSRAMInitStructure.FSMC_Bank                  = FSMC_Bank1_NORSRAMx;
	FSMC_NORSRAMInitStructure.FSMC_DataAddressMux        = FSMC_DataAddressMux_Disable;
	FSMC_NORSRAMInitStructure.FSMC_MemoryType            = FSMC_MemoryType_NOR;
	FSMC_NORSRAMInitStructure.FSMC_MemoryDataWidth       = FSMC_MemoryDataWidth_16b;
	FSMC_NORSRAMInitStructure.FSMC_BurstAccessMode       = FSMC_BurstAccessMode_Disable;
	FSMC_NORSRAMInitStructure.FSMC_WaitSignalPolarity    = FSMC_WaitSignalPolarity_Low;
	FSMC_NORSRAMInitStructure.FSMC_WrapMode              = FSMC_WrapMode_Disable;
	FSMC_NORSRAMInitStructure.FSMC_WaitSignalActive      = FSMC_WaitSignalActive_BeforeWaitState;
	FSMC_NORSRAMInitStructure.FSMC_WriteOperation        = FSMC_WriteOperation_Enable;
	FSMC_NORSRAMInitStructure.FSMC_WaitSignal            = FSMC_WaitSignal_Disable;
	FSMC_NORSRAMInitStructure.FSMC_ExtendedMode          = FSMC_ExtendedMode_Disable;
	FSMC_NORSRAMInitStructure.FSMC_WriteBurst            = FSMC_WriteBurst_Disable;
	FSMC_NORSRAMInitStructure.FSMC_ReadWriteTimingStruct = &readWriteTiming;
	FSMC_NORSRAMInitStructure.FSMC_WriteTimingStruct     = &readWriteTiming;  
	
	FSMC_NORSRAMInit ( & FSMC_NORSRAMInitStructure ); 
	
	/* 使能 FSMC_Bank1_NORSRAM3 */
	FSMC_NORSRAMCmd ( FSMC_Bank1_NORSRAMx, ENABLE );  	
}

/**
 * @brief  初始化ILI9806G寄存器
 * @param  无
 * @retval 无
 */
static void ILI9806G_REG_Config ( void )
{	
///ILI9806G-HSD43
  //PAGE1
  ILI9806G_Write_Cmd(0xF000);    ILI9806G_Write_Data(0x0055);
  ILI9806G_Write_Cmd(0xF001);    ILI9806G_Write_Data(0x00AA);
  ILI9806G_Write_Cmd(0xF002);    ILI9806G_Write_Data(0x0052);
  ILI9806G_Write_Cmd(0xF003);    ILI9806G_Write_Data(0x0008);
  ILI9806G_Write_Cmd(0xF004);    ILI9806G_Write_Data(0x0001);

  //Set AVDD 5.2V
  ILI9806G_Write_Cmd(0xB000);    ILI9806G_Write_Data(0x000D);
  ILI9806G_Write_Cmd(0xB001);    ILI9806G_Write_Data(0x000D);
  ILI9806G_Write_Cmd(0xB002);    ILI9806G_Write_Data(0x000D);

  //Set AVEE 5.2V
  ILI9806G_Write_Cmd(0xB100);    ILI9806G_Write_Data(0x000D);
  ILI9806G_Write_Cmd(0xB101);    ILI9806G_Write_Data(0x000D);
  ILI9806G_Write_Cmd(0xB102);    ILI9806G_Write_Data(0x000D);

  //Set VCL -2.5V
  ILI9806G_Write_Cmd(0xB200);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xB201);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xB202);    ILI9806G_Write_Data(0x0000);				

  //Set AVDD Ratio
  ILI9806G_Write_Cmd(0xB600);    ILI9806G_Write_Data(0x0044);
  ILI9806G_Write_Cmd(0xB601);    ILI9806G_Write_Data(0x0044);
  ILI9806G_Write_Cmd(0xB602);    ILI9806G_Write_Data(0x0044);

  //Set AVEE Ratio
  ILI9806G_Write_Cmd(0xB700);    ILI9806G_Write_Data(0x0034);
  ILI9806G_Write_Cmd(0xB701);    ILI9806G_Write_Data(0x0034);
  ILI9806G_Write_Cmd(0xB702);    ILI9806G_Write_Data(0x0034);

  //Set VCL -2.5V
  ILI9806G_Write_Cmd(0xB800);    ILI9806G_Write_Data(0x0034);
  ILI9806G_Write_Cmd(0xB801);    ILI9806G_Write_Data(0x0034);
  ILI9806G_Write_Cmd(0xB802);    ILI9806G_Write_Data(0x0034);
        
  //Control VGH booster voltage rang
  ILI9806G_Write_Cmd(0xBF00);    ILI9806G_Write_Data(0x0001); //VGH:7~18V	

  //VGH=15V(1V/step)	Free pump
  ILI9806G_Write_Cmd(0xB300);    ILI9806G_Write_Data(0x000f);		//08
  ILI9806G_Write_Cmd(0xB301);    ILI9806G_Write_Data(0x000f);		//08
  ILI9806G_Write_Cmd(0xB302);    ILI9806G_Write_Data(0x000f);		//08

  //VGH Ratio
  ILI9806G_Write_Cmd(0xB900);    ILI9806G_Write_Data(0x0034);
  ILI9806G_Write_Cmd(0xB901);    ILI9806G_Write_Data(0x0034);
  ILI9806G_Write_Cmd(0xB902);    ILI9806G_Write_Data(0x0034);

  //VGL_REG=-10(1V/step)
  ILI9806G_Write_Cmd(0xB500);    ILI9806G_Write_Data(0x0008);
  ILI9806G_Write_Cmd(0xB501);    ILI9806G_Write_Data(0x0008);
  ILI9806G_Write_Cmd(0xB502);    ILI9806G_Write_Data(0x0008);

  ILI9806G_Write_Cmd(0xC200);    ILI9806G_Write_Data(0x0003);

  //VGLX Ratio
  ILI9806G_Write_Cmd(0xBA00);    ILI9806G_Write_Data(0x0034);
  ILI9806G_Write_Cmd(0xBA01);    ILI9806G_Write_Data(0x0034);
  ILI9806G_Write_Cmd(0xBA02);    ILI9806G_Write_Data(0x0034);

    //VGMP/VGSP=4.5V/0V
  ILI9806G_Write_Cmd(0xBC00);    ILI9806G_Write_Data(0x0000);		//00
  ILI9806G_Write_Cmd(0xBC01);    ILI9806G_Write_Data(0x0078);		//C8 =5.5V/90=4.8V
  ILI9806G_Write_Cmd(0xBC02);    ILI9806G_Write_Data(0x0000);		//01

  //VGMN/VGSN=-4.5V/0V
  ILI9806G_Write_Cmd(0xBD00);    ILI9806G_Write_Data(0x0000); //00
  ILI9806G_Write_Cmd(0xBD01);    ILI9806G_Write_Data(0x0078); //90
  ILI9806G_Write_Cmd(0xBD02);    ILI9806G_Write_Data(0x0000);

  //Vcom=-1.4V(12.5mV/step)
  ILI9806G_Write_Cmd(0xBE00);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xBE01);    ILI9806G_Write_Data(0x0064); //HSD:64;Novatek:50=-1.0V, 80  5f

  //Gamma (R+)
  ILI9806G_Write_Cmd(0xD100);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD101);    ILI9806G_Write_Data(0x0033);
  ILI9806G_Write_Cmd(0xD102);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD103);    ILI9806G_Write_Data(0x0034);
  ILI9806G_Write_Cmd(0xD104);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD105);    ILI9806G_Write_Data(0x003A);
  ILI9806G_Write_Cmd(0xD106);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD107);    ILI9806G_Write_Data(0x004A);
  ILI9806G_Write_Cmd(0xD108);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD109);    ILI9806G_Write_Data(0x005C);
  ILI9806G_Write_Cmd(0xD10A);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD10B);    ILI9806G_Write_Data(0x0081);
  ILI9806G_Write_Cmd(0xD10C);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD10D);    ILI9806G_Write_Data(0x00A6);
  ILI9806G_Write_Cmd(0xD10E);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD10F);    ILI9806G_Write_Data(0x00E5);
  ILI9806G_Write_Cmd(0xD110);    ILI9806G_Write_Data(0x0001);
  ILI9806G_Write_Cmd(0xD111);    ILI9806G_Write_Data(0x0013);
  ILI9806G_Write_Cmd(0xD112);    ILI9806G_Write_Data(0x0001);
  ILI9806G_Write_Cmd(0xD113);    ILI9806G_Write_Data(0x0054);
  ILI9806G_Write_Cmd(0xD114);    ILI9806G_Write_Data(0x0001);
  ILI9806G_Write_Cmd(0xD115);    ILI9806G_Write_Data(0x0082);
  ILI9806G_Write_Cmd(0xD116);    ILI9806G_Write_Data(0x0001);
  ILI9806G_Write_Cmd(0xD117);    ILI9806G_Write_Data(0x00CA);
  ILI9806G_Write_Cmd(0xD118);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD119);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD11A);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD11B);    ILI9806G_Write_Data(0x0001);
  ILI9806G_Write_Cmd(0xD11C);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD11D);    ILI9806G_Write_Data(0x0034);
  ILI9806G_Write_Cmd(0xD11E);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD11F);    ILI9806G_Write_Data(0x0067);
  ILI9806G_Write_Cmd(0xD120);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD121);    ILI9806G_Write_Data(0x0084);
  ILI9806G_Write_Cmd(0xD122);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD123);    ILI9806G_Write_Data(0x00A4);
  ILI9806G_Write_Cmd(0xD124);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD125);    ILI9806G_Write_Data(0x00B7);
  ILI9806G_Write_Cmd(0xD126);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD127);    ILI9806G_Write_Data(0x00CF);
  ILI9806G_Write_Cmd(0xD128);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD129);    ILI9806G_Write_Data(0x00DE);
  ILI9806G_Write_Cmd(0xD12A);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD12B);    ILI9806G_Write_Data(0x00F2);
  ILI9806G_Write_Cmd(0xD12C);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD12D);    ILI9806G_Write_Data(0x00FE);
  ILI9806G_Write_Cmd(0xD12E);    ILI9806G_Write_Data(0x0003);
  ILI9806G_Write_Cmd(0xD12F);    ILI9806G_Write_Data(0x0010);
  ILI9806G_Write_Cmd(0xD130);    ILI9806G_Write_Data(0x0003);
  ILI9806G_Write_Cmd(0xD131);    ILI9806G_Write_Data(0x0033);
  ILI9806G_Write_Cmd(0xD132);    ILI9806G_Write_Data(0x0003);
  ILI9806G_Write_Cmd(0xD133);    ILI9806G_Write_Data(0x006D);

  //Gamma (G+)
  ILI9806G_Write_Cmd(0xD200);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD201);    ILI9806G_Write_Data(0x0033);
  ILI9806G_Write_Cmd(0xD202);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD203);    ILI9806G_Write_Data(0x0034);
  ILI9806G_Write_Cmd(0xD204);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD205);    ILI9806G_Write_Data(0x003A);
  ILI9806G_Write_Cmd(0xD206);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD207);    ILI9806G_Write_Data(0x004A);
  ILI9806G_Write_Cmd(0xD208);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD209);    ILI9806G_Write_Data(0x005C);
  ILI9806G_Write_Cmd(0xD20A);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD20B);    ILI9806G_Write_Data(0x0081);
  ILI9806G_Write_Cmd(0xD20C);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD20D);    ILI9806G_Write_Data(0x00A6);
  ILI9806G_Write_Cmd(0xD20E);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD20F);    ILI9806G_Write_Data(0x00E5);
  ILI9806G_Write_Cmd(0xD210);    ILI9806G_Write_Data(0x0001);
  ILI9806G_Write_Cmd(0xD211);    ILI9806G_Write_Data(0x0013);
  ILI9806G_Write_Cmd(0xD212);    ILI9806G_Write_Data(0x0001);
  ILI9806G_Write_Cmd(0xD213);    ILI9806G_Write_Data(0x0054);
  ILI9806G_Write_Cmd(0xD214);    ILI9806G_Write_Data(0x0001);
  ILI9806G_Write_Cmd(0xD215);    ILI9806G_Write_Data(0x0082);
  ILI9806G_Write_Cmd(0xD216);    ILI9806G_Write_Data(0x0001);
  ILI9806G_Write_Cmd(0xD217);    ILI9806G_Write_Data(0x00CA);
  ILI9806G_Write_Cmd(0xD218);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD219);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD21A);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD21B);    ILI9806G_Write_Data(0x0001);
  ILI9806G_Write_Cmd(0xD21C);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD21D);    ILI9806G_Write_Data(0x0034);
  ILI9806G_Write_Cmd(0xD21E);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD21F);    ILI9806G_Write_Data(0x0067);
  ILI9806G_Write_Cmd(0xD220);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD221);    ILI9806G_Write_Data(0x0084);
  ILI9806G_Write_Cmd(0xD222);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD223);    ILI9806G_Write_Data(0x00A4);
  ILI9806G_Write_Cmd(0xD224);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD225);    ILI9806G_Write_Data(0x00B7);
  ILI9806G_Write_Cmd(0xD226);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD227);    ILI9806G_Write_Data(0x00CF);
  ILI9806G_Write_Cmd(0xD228);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD229);    ILI9806G_Write_Data(0x00DE);
  ILI9806G_Write_Cmd(0xD22A);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD22B);    ILI9806G_Write_Data(0x00F2);
  ILI9806G_Write_Cmd(0xD22C);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD22D);    ILI9806G_Write_Data(0x00FE);
  ILI9806G_Write_Cmd(0xD22E);    ILI9806G_Write_Data(0x0003);
  ILI9806G_Write_Cmd(0xD22F);    ILI9806G_Write_Data(0x0010);
  ILI9806G_Write_Cmd(0xD230);    ILI9806G_Write_Data(0x0003);
  ILI9806G_Write_Cmd(0xD231);    ILI9806G_Write_Data(0x0033);
  ILI9806G_Write_Cmd(0xD232);    ILI9806G_Write_Data(0x0003);
  ILI9806G_Write_Cmd(0xD233);    ILI9806G_Write_Data(0x006D);

  //Gamma (B+)
  ILI9806G_Write_Cmd(0xD300);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD301);    ILI9806G_Write_Data(0x0033);
  ILI9806G_Write_Cmd(0xD302);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD303);    ILI9806G_Write_Data(0x0034);
  ILI9806G_Write_Cmd(0xD304);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD305);    ILI9806G_Write_Data(0x003A);
  ILI9806G_Write_Cmd(0xD306);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD307);    ILI9806G_Write_Data(0x004A);
  ILI9806G_Write_Cmd(0xD308);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD309);    ILI9806G_Write_Data(0x005C);
  ILI9806G_Write_Cmd(0xD30A);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD30B);    ILI9806G_Write_Data(0x0081);
  ILI9806G_Write_Cmd(0xD30C);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD30D);    ILI9806G_Write_Data(0x00A6);
  ILI9806G_Write_Cmd(0xD30E);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD30F);    ILI9806G_Write_Data(0x00E5);
  ILI9806G_Write_Cmd(0xD310);    ILI9806G_Write_Data(0x0001);
  ILI9806G_Write_Cmd(0xD311);    ILI9806G_Write_Data(0x0013);
  ILI9806G_Write_Cmd(0xD312);    ILI9806G_Write_Data(0x0001);
  ILI9806G_Write_Cmd(0xD313);    ILI9806G_Write_Data(0x0054);
  ILI9806G_Write_Cmd(0xD314);    ILI9806G_Write_Data(0x0001);
  ILI9806G_Write_Cmd(0xD315);    ILI9806G_Write_Data(0x0082);
  ILI9806G_Write_Cmd(0xD316);    ILI9806G_Write_Data(0x0001);
  ILI9806G_Write_Cmd(0xD317);    ILI9806G_Write_Data(0x00CA);
  ILI9806G_Write_Cmd(0xD318);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD319);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD31A);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD31B);    ILI9806G_Write_Data(0x0001);
  ILI9806G_Write_Cmd(0xD31C);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD31D);    ILI9806G_Write_Data(0x0034);
  ILI9806G_Write_Cmd(0xD31E);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD31F);    ILI9806G_Write_Data(0x0067);
  ILI9806G_Write_Cmd(0xD320);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD321);    ILI9806G_Write_Data(0x0084);
  ILI9806G_Write_Cmd(0xD322);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD323);    ILI9806G_Write_Data(0x00A4);
  ILI9806G_Write_Cmd(0xD324);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD325);    ILI9806G_Write_Data(0x00B7);
  ILI9806G_Write_Cmd(0xD326);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD327);    ILI9806G_Write_Data(0x00CF);
  ILI9806G_Write_Cmd(0xD328);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD329);    ILI9806G_Write_Data(0x00DE);
  ILI9806G_Write_Cmd(0xD32A);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD32B);    ILI9806G_Write_Data(0x00F2);
  ILI9806G_Write_Cmd(0xD32C);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD32D);    ILI9806G_Write_Data(0x00FE);
  ILI9806G_Write_Cmd(0xD32E);    ILI9806G_Write_Data(0x0003);
  ILI9806G_Write_Cmd(0xD32F);    ILI9806G_Write_Data(0x0010);
  ILI9806G_Write_Cmd(0xD330);    ILI9806G_Write_Data(0x0003);
  ILI9806G_Write_Cmd(0xD331);    ILI9806G_Write_Data(0x0033);
  ILI9806G_Write_Cmd(0xD332);    ILI9806G_Write_Data(0x0003);
  ILI9806G_Write_Cmd(0xD333);    ILI9806G_Write_Data(0x006D);

  //Gamma (R-)
  ILI9806G_Write_Cmd(0xD400);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD401);    ILI9806G_Write_Data(0x0033);
  ILI9806G_Write_Cmd(0xD402);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD403);    ILI9806G_Write_Data(0x0034);
  ILI9806G_Write_Cmd(0xD404);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD405);    ILI9806G_Write_Data(0x003A);
  ILI9806G_Write_Cmd(0xD406);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD407);    ILI9806G_Write_Data(0x004A);
  ILI9806G_Write_Cmd(0xD408);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD409);    ILI9806G_Write_Data(0x005C);
  ILI9806G_Write_Cmd(0xD40A);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD40B);    ILI9806G_Write_Data(0x0081);
  ILI9806G_Write_Cmd(0xD40C);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD40D);    ILI9806G_Write_Data(0x00A6);
  ILI9806G_Write_Cmd(0xD40E);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD40F);    ILI9806G_Write_Data(0x00E5);
  ILI9806G_Write_Cmd(0xD410);    ILI9806G_Write_Data(0x0001);
  ILI9806G_Write_Cmd(0xD411);    ILI9806G_Write_Data(0x0013);
  ILI9806G_Write_Cmd(0xD412);    ILI9806G_Write_Data(0x0001);
  ILI9806G_Write_Cmd(0xD413);    ILI9806G_Write_Data(0x0054);
  ILI9806G_Write_Cmd(0xD414);    ILI9806G_Write_Data(0x0001);
  ILI9806G_Write_Cmd(0xD415);    ILI9806G_Write_Data(0x0082);
  ILI9806G_Write_Cmd(0xD416);    ILI9806G_Write_Data(0x0001);
  ILI9806G_Write_Cmd(0xD417);    ILI9806G_Write_Data(0x00CA);
  ILI9806G_Write_Cmd(0xD418);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD419);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD41A);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD41B);    ILI9806G_Write_Data(0x0001);
  ILI9806G_Write_Cmd(0xD41C);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD41D);    ILI9806G_Write_Data(0x0034);
  ILI9806G_Write_Cmd(0xD41E);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD41F);    ILI9806G_Write_Data(0x0067);
  ILI9806G_Write_Cmd(0xD420);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD421);    ILI9806G_Write_Data(0x0084);
  ILI9806G_Write_Cmd(0xD422);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD423);    ILI9806G_Write_Data(0x00A4);
  ILI9806G_Write_Cmd(0xD424);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD425);    ILI9806G_Write_Data(0x00B7);
  ILI9806G_Write_Cmd(0xD426);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD427);    ILI9806G_Write_Data(0x00CF);
  ILI9806G_Write_Cmd(0xD428);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD429);    ILI9806G_Write_Data(0x00DE);
  ILI9806G_Write_Cmd(0xD42A);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD42B);    ILI9806G_Write_Data(0x00F2);
  ILI9806G_Write_Cmd(0xD42C);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD42D);    ILI9806G_Write_Data(0x00FE);
  ILI9806G_Write_Cmd(0xD42E);    ILI9806G_Write_Data(0x0003);
  ILI9806G_Write_Cmd(0xD42F);    ILI9806G_Write_Data(0x0010);
  ILI9806G_Write_Cmd(0xD430);    ILI9806G_Write_Data(0x0003);
  ILI9806G_Write_Cmd(0xD431);    ILI9806G_Write_Data(0x0033);
  ILI9806G_Write_Cmd(0xD432);    ILI9806G_Write_Data(0x0003);
  ILI9806G_Write_Cmd(0xD433);    ILI9806G_Write_Data(0x006D);

  //Gamma (G-)
  ILI9806G_Write_Cmd(0xD500);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD501);    ILI9806G_Write_Data(0x0033);
  ILI9806G_Write_Cmd(0xD502);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD503);    ILI9806G_Write_Data(0x0034);
  ILI9806G_Write_Cmd(0xD504);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD505);    ILI9806G_Write_Data(0x003A);
  ILI9806G_Write_Cmd(0xD506);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD507);    ILI9806G_Write_Data(0x004A);
  ILI9806G_Write_Cmd(0xD508);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD509);    ILI9806G_Write_Data(0x005C);
  ILI9806G_Write_Cmd(0xD50A);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD50B);    ILI9806G_Write_Data(0x0081);
  ILI9806G_Write_Cmd(0xD50C);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD50D);    ILI9806G_Write_Data(0x00A6);
  ILI9806G_Write_Cmd(0xD50E);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD50F);    ILI9806G_Write_Data(0x00E5);
  ILI9806G_Write_Cmd(0xD510);    ILI9806G_Write_Data(0x0001);
  ILI9806G_Write_Cmd(0xD511);    ILI9806G_Write_Data(0x0013);
  ILI9806G_Write_Cmd(0xD512);    ILI9806G_Write_Data(0x0001);
  ILI9806G_Write_Cmd(0xD513);    ILI9806G_Write_Data(0x0054);
  ILI9806G_Write_Cmd(0xD514);    ILI9806G_Write_Data(0x0001);
  ILI9806G_Write_Cmd(0xD515);    ILI9806G_Write_Data(0x0082);
  ILI9806G_Write_Cmd(0xD516);    ILI9806G_Write_Data(0x0001);
  ILI9806G_Write_Cmd(0xD517);    ILI9806G_Write_Data(0x00CA);
  ILI9806G_Write_Cmd(0xD518);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD519);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD51A);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD51B);    ILI9806G_Write_Data(0x0001);
  ILI9806G_Write_Cmd(0xD51C);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD51D);    ILI9806G_Write_Data(0x0034);
  ILI9806G_Write_Cmd(0xD51E);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD51F);    ILI9806G_Write_Data(0x0067);
  ILI9806G_Write_Cmd(0xD520);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD521);    ILI9806G_Write_Data(0x0084);
  ILI9806G_Write_Cmd(0xD522);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD523);    ILI9806G_Write_Data(0x00A4);
  ILI9806G_Write_Cmd(0xD524);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD525);    ILI9806G_Write_Data(0x00B7);
  ILI9806G_Write_Cmd(0xD526);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD527);    ILI9806G_Write_Data(0x00CF);
  ILI9806G_Write_Cmd(0xD528);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD529);    ILI9806G_Write_Data(0x00DE);
  ILI9806G_Write_Cmd(0xD52A);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD52B);    ILI9806G_Write_Data(0x00F2);
  ILI9806G_Write_Cmd(0xD52C);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD52D);    ILI9806G_Write_Data(0x00FE);
  ILI9806G_Write_Cmd(0xD52E);    ILI9806G_Write_Data(0x0003);
  ILI9806G_Write_Cmd(0xD52F);    ILI9806G_Write_Data(0x0010);
  ILI9806G_Write_Cmd(0xD530);    ILI9806G_Write_Data(0x0003);
  ILI9806G_Write_Cmd(0xD531);    ILI9806G_Write_Data(0x0033);
  ILI9806G_Write_Cmd(0xD532);    ILI9806G_Write_Data(0x0003);
  ILI9806G_Write_Cmd(0xD533);    ILI9806G_Write_Data(0x006D);

  //Gamma (B-)
  ILI9806G_Write_Cmd(0xD600);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD601);    ILI9806G_Write_Data(0x0033);
  ILI9806G_Write_Cmd(0xD602);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD603);    ILI9806G_Write_Data(0x0034);
  ILI9806G_Write_Cmd(0xD604);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD605);    ILI9806G_Write_Data(0x003A);
  ILI9806G_Write_Cmd(0xD606);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD607);    ILI9806G_Write_Data(0x004A);
  ILI9806G_Write_Cmd(0xD608);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD609);    ILI9806G_Write_Data(0x005C);
  ILI9806G_Write_Cmd(0xD60A);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD60B);    ILI9806G_Write_Data(0x0081);
  ILI9806G_Write_Cmd(0xD60C);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD60D);    ILI9806G_Write_Data(0x00A6);
  ILI9806G_Write_Cmd(0xD60E);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD60F);    ILI9806G_Write_Data(0x00E5);
  ILI9806G_Write_Cmd(0xD610);    ILI9806G_Write_Data(0x0001);
  ILI9806G_Write_Cmd(0xD611);    ILI9806G_Write_Data(0x0013);
  ILI9806G_Write_Cmd(0xD612);    ILI9806G_Write_Data(0x0001);
  ILI9806G_Write_Cmd(0xD613);    ILI9806G_Write_Data(0x0054);
  ILI9806G_Write_Cmd(0xD614);    ILI9806G_Write_Data(0x0001);
  ILI9806G_Write_Cmd(0xD615);    ILI9806G_Write_Data(0x0082);
  ILI9806G_Write_Cmd(0xD616);    ILI9806G_Write_Data(0x0001);
  ILI9806G_Write_Cmd(0xD617);    ILI9806G_Write_Data(0x00CA);
  ILI9806G_Write_Cmd(0xD618);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD619);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0xD61A);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD61B);    ILI9806G_Write_Data(0x0001);
  ILI9806G_Write_Cmd(0xD61C);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD61D);    ILI9806G_Write_Data(0x0034);
  ILI9806G_Write_Cmd(0xD61E);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD61F);    ILI9806G_Write_Data(0x0067);
  ILI9806G_Write_Cmd(0xD620);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD621);    ILI9806G_Write_Data(0x0084);
  ILI9806G_Write_Cmd(0xD622);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD623);    ILI9806G_Write_Data(0x00A4);
  ILI9806G_Write_Cmd(0xD624);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD625);    ILI9806G_Write_Data(0x00B7);
  ILI9806G_Write_Cmd(0xD626);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD627);    ILI9806G_Write_Data(0x00CF);
  ILI9806G_Write_Cmd(0xD628);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD629);    ILI9806G_Write_Data(0x00DE);
  ILI9806G_Write_Cmd(0xD62A);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD62B);    ILI9806G_Write_Data(0x00F2);
  ILI9806G_Write_Cmd(0xD62C);    ILI9806G_Write_Data(0x0002);
  ILI9806G_Write_Cmd(0xD62D);    ILI9806G_Write_Data(0x00FE);
  ILI9806G_Write_Cmd(0xD62E);    ILI9806G_Write_Data(0x0003);
  ILI9806G_Write_Cmd(0xD62F);    ILI9806G_Write_Data(0x0010);
  ILI9806G_Write_Cmd(0xD630);    ILI9806G_Write_Data(0x0003);
  ILI9806G_Write_Cmd(0xD631);    ILI9806G_Write_Data(0x0033);
  ILI9806G_Write_Cmd(0xD632);    ILI9806G_Write_Data(0x0003);
  ILI9806G_Write_Cmd(0xD633);    ILI9806G_Write_Data(0x006D);

  //PAGE0
  ILI9806G_Write_Cmd(0xF000);    ILI9806G_Write_Data(0x0055);
  ILI9806G_Write_Cmd(0xF001);    ILI9806G_Write_Data(0x00AA);
  ILI9806G_Write_Cmd(0xF002);    ILI9806G_Write_Data(0x0052);
  ILI9806G_Write_Cmd(0xF003);    ILI9806G_Write_Data(0x0008);	
  ILI9806G_Write_Cmd(0xF004);    ILI9806G_Write_Data(0x0000); 

  //480x800
  ILI9806G_Write_Cmd(0xB500);    ILI9806G_Write_Data(0x0050);

  //ILI9806G_Write_Cmd(0x2C00);    ILI9806G_Write_Data(0x0006); //8BIT 6-6-6?

  //Dispay control
  ILI9806G_Write_Cmd(0xB100);    ILI9806G_Write_Data(0x00CC);	
  ILI9806G_Write_Cmd(0xB101);    ILI9806G_Write_Data(0x0000); // S1->S1440:00;S1440->S1:02

  //Source hold time (Nova non-used)
  ILI9806G_Write_Cmd(0xB600);    ILI9806G_Write_Data(0x0005);

  //Gate EQ control	 (Nova non-used)
  ILI9806G_Write_Cmd(0xB700);    ILI9806G_Write_Data(0x0077);  //HSD:70;Nova:77	 
  ILI9806G_Write_Cmd(0xB701);    ILI9806G_Write_Data(0x0077);	//HSD:70;Nova:77

  //Source EQ control (Nova non-used)
  ILI9806G_Write_Cmd(0xB800);    ILI9806G_Write_Data(0x0001);  
  ILI9806G_Write_Cmd(0xB801);    ILI9806G_Write_Data(0x0003);	//HSD:05;Nova:07
  ILI9806G_Write_Cmd(0xB802);    ILI9806G_Write_Data(0x0003);	//HSD:05;Nova:07
  ILI9806G_Write_Cmd(0xB803);    ILI9806G_Write_Data(0x0003);	//HSD:05;Nova:07

  //Inversion mode: column
  ILI9806G_Write_Cmd(0xBC00);    ILI9806G_Write_Data(0x0002);	//00: column
  ILI9806G_Write_Cmd(0xBC01);    ILI9806G_Write_Data(0x0000);	//01:1dot
  ILI9806G_Write_Cmd(0xBC02);    ILI9806G_Write_Data(0x0000); 

  //Frame rate	(Nova non-used)
  ILI9806G_Write_Cmd(0xBD00);    ILI9806G_Write_Data(0x0001);
  ILI9806G_Write_Cmd(0xBD01);    ILI9806G_Write_Data(0x0084);
  ILI9806G_Write_Cmd(0xBD02);    ILI9806G_Write_Data(0x001c); //HSD:06;Nova:1C
  ILI9806G_Write_Cmd(0xBD03);    ILI9806G_Write_Data(0x001c); //HSD:04;Nova:1C
  ILI9806G_Write_Cmd(0xBD04);    ILI9806G_Write_Data(0x0000);

  //LGD timing control(4H/4-delay_ms)
  ILI9806G_Write_Cmd(0xC900);    ILI9806G_Write_Data(0x00D0);	//3H:0x50;4H:0xD0	 //D
  ILI9806G_Write_Cmd(0xC901);    ILI9806G_Write_Data(0x0002);  //HSD:05;Nova:02
  ILI9806G_Write_Cmd(0xC902);    ILI9806G_Write_Data(0x0050);	//HSD:05;Nova:50
  ILI9806G_Write_Cmd(0xC903);    ILI9806G_Write_Data(0x0050);	//HSD:05;Nova:50	;STV delay_ms time
  ILI9806G_Write_Cmd(0xC904);    ILI9806G_Write_Data(0x0050);	//HSD:05;Nova:50	;CLK delay_ms time

  ILI9806G_Write_Cmd(0x3600);    ILI9806G_Write_Data(0x0000);
  ILI9806G_Write_Cmd(0x3500);    ILI9806G_Write_Data(0x0000);

  ILI9806G_Write_Cmd(0xFF00);    ILI9806G_Write_Data(0x00AA);
  ILI9806G_Write_Cmd(0xFF01);    ILI9806G_Write_Data(0x0055);
  ILI9806G_Write_Cmd(0xFF02);    ILI9806G_Write_Data(0x0025);
  ILI9806G_Write_Cmd(0xFF03);    ILI9806G_Write_Data(0x0001);

  ILI9806G_Write_Cmd(0xFC00);    ILI9806G_Write_Data(0x0016);
  ILI9806G_Write_Cmd(0xFC01);    ILI9806G_Write_Data(0x00A2);
  ILI9806G_Write_Cmd(0xFC02);    ILI9806G_Write_Data(0x0026);
  ILI9806G_Write_Cmd(0x3A00);    ILI9806G_Write_Data(0x0006);

  ILI9806G_Write_Cmd(0x3A00);    ILI9806G_Write_Data(0x0055);
  //Sleep out
  ILI9806G_Write_Cmd(0x1100);	   // Sleep out
  Delay_ms(120U);

  //Display on
  ILI9806G_Write_Cmd(0x2900);
  Delay_ms(20U);
}


/**
 * @brief  ILI9806G初始化
 * @param  无
 * @retval 无
 */
void ILI9806G_Init ( void )
{
	ILI9806G_GPIO_Config();
	ILI9806G_FSMC_Config();
	
	ILI9806G_Rst();
	ILI9806G_REG_Config();
	
	//设置默认扫描方向，其中 6 模式为大部分液晶例程的默认显示方向  
	ILI9806G_GramScan(LCD_SCAN_MODE);
    
	ILI9806G_Clear(0,0,LCD_X_LENGTH,LCD_Y_LENGTH);	//清屏，显示全黑
	ILI9806G_BackLed_Control ( ENABLE );      //点亮LCD背光灯
}

//RGB888   == 红色 0xFF0000
//绘制矩形 RGB565   
void LCD_Draw_Rect(uint16_t x0,uint16_t x1,uint16_t y0,uint16_t y1,uint16_t color)
{
    uint32_t pixel_count;

    if ((x1 < x0) || (y1 < y0))
        return;

    ILI9806G_OpenWindow(x0, y0, (uint16_t)(x1 - x0 + 1U), (uint16_t)(y1 - y0 + 1U));
    pixel_count = (uint32_t)(x1 - x0 + 1U) * (uint32_t)(y1 - y0 + 1U);
    ILI9806G_Write_Cmd(CMD_SetPixel);
    while (pixel_count-- != 0U)
        ILI9806G_Write_Data(color);
}


/**
 * @brief  ILI9806G背光LED控制
 * @param  enumState ：决定是否使能背光LED
  *   该参数为以下值之一：
  *     @arg ENABLE :使能背光LED
  *     @arg DISABLE :禁用背光LED
 * @retval 无
 */
void ILI9806G_BackLed_Control ( FunctionalState enumState )
{
	if ( enumState )
		 GPIO_SetBits( ILI9806G_BK_PORT, ILI9806G_BK_PIN );	
	else
		 GPIO_ResetBits( ILI9806G_BK_PORT, ILI9806G_BK_PIN );
}



/**
 * @brief  ILI9806G 软件复位
 * @param  无
 * @retval 无
 */
void ILI9806G_Rst ( void )
{
    GPIO_ResetBits(ILI9806G_RST_PORT, ILI9806G_RST_PIN);
    Delay_ms(20U);

    GPIO_SetBits(ILI9806G_RST_PORT, ILI9806G_RST_PIN);
    Delay_ms(120U);
}




/**
 * @brief  设置ILI9806G的GRAM的扫描方向 
 * @param  ucOption ：选择GRAM的扫描方向 
 *     @arg 0-7 :参数可选值为0-7这八个方向
 *
 *	！！！其中0、3、5、6 模式适合从左至右显示文字，
 *				不推荐使用其它模式显示文字	其它模式显示文字会有镜像效果			
 *		
 *	其中0、2、4、6 模式的X方向像素为480，Y方向像素为854
 *	其中1、3、5、7 模式下X方向像素为854，Y方向像素为480
 *
 *	其中 6 模式为大部分液晶例程的默认显示方向
 *	其中 3 模式为摄像头例程使用的方向
 *	其中 0 模式为BMP图片显示例程使用的方向
 *
 * @retval 无
 * @note  坐标图例：A表示向上，V表示向下，<表示向左，>表示向右
					X表示X轴，Y表示Y轴

------------------------------------------------------------
模式0：				.		模式1：		.	模式2：			.	模式3：					
					A		.					A		.		A					.		A									
					|		.					|		.		|					.		|							
					Y		.					X		.		Y					.		X					
					0		.					1		.		2					.		3					
	<--- X0 o		.	<----Y1	o		.		o 2X--->  .		o 3Y--->	
------------------------------------------------------------	
模式4：				.	模式5：			.	模式6：			.	模式7：					
	<--- X4 o		.	<--- Y5 o		.		o 6X--->  .		o 7Y--->	
					4		.					5		.		6					.		7	
					Y		.					X		.		Y					.		X						
					|		.					|		.		|					.		|							
					V		.					V		.		V					.		V		
---------------------------------------------------------				
											 LCD屏示例
								|-----------------|
								|			野火Logo		|
								|									|
								|									|
								|									|
								|									|
								|									|
								|									|
								|									|
								|									|
								|-----------------|
								屏幕正面（宽480，高854）

 *******************************************************/
void ILI9806G_GramScan ( uint8_t ucOption )
{	
	//参数检查，只可输入0-7
	if(ucOption >7 )
		return;
	
	//根据模式更新LCD_SCAN_MODE的值，主要用于触摸屏选择计算参数
	LCD_SCAN_MODE = ucOption;
	
	//根据模式更新XY方向的像素宽度
	if(ucOption%2 == 0)	
	{
		//0 2 4 6模式下X方向像素宽度为480，Y方向为854
		LCD_X_LENGTH = ILI9806G_LESS_PIXEL;
		LCD_Y_LENGTH =	ILI9806G_MORE_PIXEL;
	}
	else				
	{
		//1 3 5 7模式下X方向像素宽度为854，Y方向为480
		LCD_X_LENGTH = ILI9806G_MORE_PIXEL;
		LCD_Y_LENGTH =	ILI9806G_LESS_PIXEL; 
	}

	//0x36命令参数的高3位可用于设置GRAM扫描方向	
	ILI9806G_Write_Cmd ( 0x3600 ); 
	ILI9806G_Write_Data (0x00 | (ucOption<<5));//根据ucOption的值设置LCD参数，共0-7种模式
	ILI9806G_Write_Cmd ( CMD_SetCoordinateX ); 
	ILI9806G_Write_Data ( 0x00 );		/* x 起始坐标高8位 */
  ILI9806G_Write_Cmd ( CMD_SetCoordinateX + 1 ); 
	ILI9806G_Write_Data ( 0x00 );		/* x 起始坐标低8位 */
  ILI9806G_Write_Cmd ( CMD_SetCoordinateX + 2 ); 
	ILI9806G_Write_Data ( ((LCD_X_LENGTH-1)>>8)&0xFF ); /* x 结束坐标高8位 */	
  ILI9806G_Write_Cmd ( CMD_SetCoordinateX + 3 ); 
	ILI9806G_Write_Data ( (LCD_X_LENGTH-1)&0xFF );				/* x 结束坐标低8位 */

	ILI9806G_Write_Cmd ( CMD_SetCoordinateY ); 
	ILI9806G_Write_Data ( 0x00 );		/* y 起始坐标高8位 */
  ILI9806G_Write_Cmd ( CMD_SetCoordinateY + 1 ); 
	ILI9806G_Write_Data ( 0x00 );		/* y 起始坐标低8位 */
  ILI9806G_Write_Cmd ( CMD_SetCoordinateY + 2 ); 
	ILI9806G_Write_Data ( ((LCD_Y_LENGTH-1)>>8)&0xFF );	/* y 结束坐标高8位 */	 
  ILI9806G_Write_Cmd ( CMD_SetCoordinateY + 3 ); 
	ILI9806G_Write_Data ( (LCD_Y_LENGTH-1)&0xFF );				/* y 结束坐标低8位 */

	/* write gram start */
	ILI9806G_Write_Cmd ( CMD_SetPixel );	
}


/**
 * @brief  在ILI9806G显示器上开辟一个窗口
 * @param  usX ：在特定扫描方向下窗口的起点X坐标
 * @param  usY ：在特定扫描方向下窗口的起点Y坐标
 * @param  usWidth ：窗口的宽度
 * @param  usHeight ：窗口的高度
 * @retval 无
 */
void ILI9806G_OpenWindow ( uint16_t usX, uint16_t usY, uint16_t usWidth, uint16_t usHeight )
{	
	ILI9806G_Write_Cmd ( CMD_SetCoordinateX ); 				 /* 设置X坐标 */
	ILI9806G_Write_Data ( usX >> 8  );	 /* 先高8位，然后低8位 */
  ILI9806G_Write_Cmd ( CMD_SetCoordinateX + 1 ); 
	ILI9806G_Write_Data ( usX & 0xff  );	 /* 设置起始点和结束点*/
  ILI9806G_Write_Cmd ( CMD_SetCoordinateX + 2 );
	ILI9806G_Write_Data ( ( usX + usWidth - 1 ) >> 8  );
  ILI9806G_Write_Cmd ( CMD_SetCoordinateX + 3 );
	ILI9806G_Write_Data ( ( usX + usWidth - 1 ) & 0xff  );

	ILI9806G_Write_Cmd ( CMD_SetCoordinateY ); 			     /* 设置Y坐标*/
	ILI9806G_Write_Data ( usY >> 8  );
  ILI9806G_Write_Cmd ( CMD_SetCoordinateY + 1);
	ILI9806G_Write_Data ( usY & 0xff  );
  ILI9806G_Write_Cmd ( CMD_SetCoordinateY + 2);
	ILI9806G_Write_Data ( ( usY + usHeight - 1 ) >> 8 );
  ILI9806G_Write_Cmd ( CMD_SetCoordinateY + 3);
	ILI9806G_Write_Data ( ( usY + usHeight - 1) & 0xff );
	
}


/**
 * @brief  设定ILI9806G的光标坐标
 * @param  usX ：在特定扫描方向下光标的X坐标
 * @param  usY ：在特定扫描方向下光标的Y坐标
 * @retval 无
 */
static void ILI9806G_SetCursor ( uint16_t usX, uint16_t usY )	
{
	ILI9806G_OpenWindow ( usX, usY, 1, 1 );
}


/**
 * @brief  在ILI9806G显示器上以某一颜色填充像素点
 * @param  ulAmout_Point ：要填充颜色的像素点的总数目
 * @param  usColor ：颜色
 * @retval 无
 */
static __inline void ILI9806G_FillColor ( uint32_t ulAmout_Point, uint16_t usColor )
{
	uint32_t i = 0;
	
	/* memory write */
	ILI9806G_Write_Cmd ( CMD_SetPixel );	
		
	for ( i = 0; i < ulAmout_Point; i ++ )
		ILI9806G_Write_Data ( usColor );
}


/**
 * @brief  对ILI9806G显示器的某一窗口以某种颜色进行清屏
 * @param  usX ：在特定扫描方向下窗口的起点X坐标
 * @param  usY ：在特定扫描方向下窗口的起点Y坐标
 * @param  usWidth ：窗口的宽度
 * @param  usHeight ：窗口的高度
 * @note 可使用LCD_SetBackColor、LCD_SetTextColor、LCD_SetColors函数设置颜色
 * @retval 无
 */
void ILI9806G_Clear ( uint16_t usX, uint16_t usY, uint16_t usWidth, uint16_t usHeight )
{
	ILI9806G_OpenWindow ( usX, usY, usWidth, usHeight );

	ILI9806G_FillColor ( usWidth * usHeight, CurrentBackColor );		
}


/**
 * @brief  对ILI9806G显示器的某一点以某种颜色进行填充
 * @param  usX ：在特定扫描方向下该点的X坐标
 * @param  usY ：在特定扫描方向下该点的Y坐标
 * @note 可使用LCD_SetBackColor、LCD_SetTextColor、LCD_SetColors函数设置颜色
 * @retval 无
 */
void ILI9806G_SetPointPixel ( uint16_t usX, uint16_t usY )	
{	
	if ( ( usX < LCD_X_LENGTH ) && ( usY < LCD_Y_LENGTH ) )
	{
		ILI9806G_SetCursor ( usX, usY );
		
		ILI9806G_FillColor ( 1, CurrentTextColor );
	}
}

void ILI9806G_DrawPoint ( uint16_t usX, uint16_t usY ,uint16_t color)	
{	
	if ( ( usX < LCD_X_LENGTH ) && ( usY < LCD_Y_LENGTH ) )
	{
		ILI9806G_SetCursor ( usX, usY );
		
		ILI9806G_FillColor ( 1, color );
	}
}


/**
 * @brief  读取ILI9806G GRAM 的一个像素数据
 * @param  无
 * @retval 像素数据
 */
static uint16_t ILI9806G_Read_PixelData ( void )	
{	
	uint16_t us_RG=0, usB=0 ;

	
	ILI9806G_Write_Cmd ( 0x2E00 );   /* 读数据 */
	//前读取三次结果去掉
	us_RG = ILI9806G_Read_Data (); 	/*FIRST READ OUT DUMMY DATA*/
	us_RG = ILI9806G_Read_Data (); 	/*FIRST READ OUT DUMMY DATA*/
	us_RG = ILI9806G_Read_Data (); 	/*FIRST READ OUT DUMMY DATA*/
	
	us_RG = ILI9806G_Read_Data ();  	/*READ OUT RED AND GREEN DATA  */
	usB = ILI9806G_Read_Data ();  		/*READ OUT BLUE DATA*/
	
  return   (us_RG&0xF800)| ((us_RG<<3)&0x7E0) | (usB>>11) ;
	
}


/**
 * @brief  获取 ILI9806G 显示器上某一个坐标点的像素数据
 * @param  usX ：在特定扫描方向下该点的X坐标
 * @param  usY ：在特定扫描方向下该点的Y坐标
 * @retval 像素数据
 */
uint16_t ILI9806G_GetPointPixel ( uint16_t usX, uint16_t usY )
{ 
	uint16_t usPixelData;
	
	ILI9806G_SetCursor ( usX, usY );
	
	usPixelData = ILI9806G_Read_PixelData ();
	
	return usPixelData;
}


/**
 * @brief  在 ILI9806G 显示器上使用 Bresenham 算法画线段 
 * @param  usX1 ：在特定扫描方向下线段的一个端点X坐标
 * @param  usY1 ：在特定扫描方向下线段的一个端点Y坐标
 * @param  usX2 ：在特定扫描方向下线段的另一个端点X坐标
 * @param  usY2 ：在特定扫描方向下线段的另一个端点Y坐标
 * @note 可使用LCD_SetBackColor、LCD_SetTextColor、LCD_SetColors函数设置颜色
 * @retval 无
 */
void ILI9806G_DrawLine ( uint16_t usX1, uint16_t usY1, uint16_t usX2, uint16_t usY2 )
{
	uint16_t us; 
	uint16_t usX_Current, usY_Current;
	
	int32_t lError_X = 0, lError_Y = 0, lDelta_X, lDelta_Y, lDistance; 
	int32_t lIncrease_X, lIncrease_Y; 	
	
	lDelta_X = usX2 - usX1; //计算坐标增量 
	lDelta_Y = usY2 - usY1; 
	
	usX_Current = usX1; 
	usY_Current = usY1; 
	
	if ( lDelta_X > 0 ) 
		lIncrease_X = 1; //设置单步方向 
	
	else if ( lDelta_X == 0 ) 
		lIncrease_X = 0;//垂直线 
	
	else 
	{ 
		lIncrease_X = -1;
		lDelta_X = - lDelta_X;
	} 

	if ( lDelta_Y > 0 )
		lIncrease_Y = 1; 
	
	else if ( lDelta_Y == 0 )
		lIncrease_Y = 0;//水平线 
	
	else 
	{
		lIncrease_Y = -1;
		lDelta_Y = - lDelta_Y;
	} 
	
	if (  lDelta_X > lDelta_Y )
		lDistance = lDelta_X; //选取基本增量坐标轴 
	
	else 
		lDistance = lDelta_Y; 

	for ( us = 0; us <= lDistance + 1; us ++ )//画线输出 
	{  
		ILI9806G_SetPointPixel ( usX_Current, usY_Current );//画点 
		
		lError_X += lDelta_X ; 
		lError_Y += lDelta_Y ; 
		
		if ( lError_X > lDistance ) 
		{ 
			lError_X -= lDistance; 
			usX_Current += lIncrease_X; 
		}  
		
		if ( lError_Y > lDistance ) 
		{ 
			lError_Y -= lDistance; 
			usY_Current += lIncrease_Y; 
		} 
	}
}   


/**
 * @brief  在 ILI9806G 显示器上画一个矩形
 * @param  usX_Start ：在特定扫描方向下矩形的起始点X坐标
 * @param  usY_Start ：在特定扫描方向下矩形的起始点Y坐标
 * @param  usWidth：矩形的宽度（单位：像素）
 * @param  usHeight：矩形的高度（单位：像素）
 * @param  ucFilled ：选择是否填充该矩形
  *   该参数为以下值之一：
  *     @arg 0 :空心矩形
  *     @arg 1 :实心矩形 
 * @note 可使用LCD_SetBackColor、LCD_SetTextColor、LCD_SetColors函数设置颜色
 * @retval 无
 */
void ILI9806G_DrawRectangle ( uint16_t usX_Start, uint16_t usY_Start, uint16_t usWidth, uint16_t usHeight, uint8_t ucFilled )
{
	if ( ucFilled )
	{
		ILI9806G_OpenWindow ( usX_Start, usY_Start, usWidth, usHeight );
		ILI9806G_FillColor ( usWidth * usHeight ,CurrentTextColor);	
	}
	else
	{
		ILI9806G_DrawLine ( usX_Start, usY_Start, usX_Start + usWidth - 1, usY_Start );
		ILI9806G_DrawLine ( usX_Start, usY_Start + usHeight - 1, usX_Start + usWidth - 1, usY_Start + usHeight - 1 );
		ILI9806G_DrawLine ( usX_Start, usY_Start, usX_Start, usY_Start + usHeight - 1 );
		ILI9806G_DrawLine ( usX_Start + usWidth - 1, usY_Start, usX_Start + usWidth - 1, usY_Start + usHeight - 1 );		
	}

}

void ILI9806G_Fill ( uint16_t usX_Start, uint16_t usY_Start, uint16_t usX_End, uint16_t usY_End, uint16_t color)
{
	ILI9806G_OpenWindow ( usX_Start, usY_Start, (usX_End-usX_Start), (usY_End-usY_Start) );
	ILI9806G_FillColor ( (usX_End-usX_Start)*(usY_End-usY_Start) ,color);	
}

/**
 * @brief  在 ILI9806G 显示器上使用 Bresenham 算法画圆
 * @param  usX_Center ：在特定扫描方向下圆心的X坐标
 * @param  usY_Center ：在特定扫描方向下圆心的Y坐标
 * @param  usRadius：圆的半径（单位：像素）
 * @param  ucFilled ：选择是否填充该圆
  *   该参数为以下值之一：
  *     @arg 0 :空心圆
  *     @arg 1 :实心圆
 * @note 可使用LCD_SetBackColor、LCD_SetTextColor、LCD_SetColors函数设置颜色
 * @retval 无
 */
void ILI9806G_DrawCircle ( uint16_t usX_Center, uint16_t usY_Center, uint16_t usRadius, uint8_t ucFilled )
{
	int16_t sCurrentX, sCurrentY;
	int16_t sError;
	
	sCurrentX = 0; sCurrentY = usRadius;	  
	
	sError = 3 - ( usRadius << 1 );     //判断下个点位置的标志
	
	while ( sCurrentX <= sCurrentY )
	{
		int16_t sCountY;
		
		
		if ( ucFilled ) 			
			for ( sCountY = sCurrentX; sCountY <= sCurrentY; sCountY ++ ) 
			{                      
				ILI9806G_SetPointPixel ( usX_Center + sCurrentX, usY_Center + sCountY );           //1，研究对象 
				ILI9806G_SetPointPixel ( usX_Center - sCurrentX, usY_Center + sCountY );           //2       
				ILI9806G_SetPointPixel ( usX_Center - sCountY,   usY_Center + sCurrentX );           //3
				ILI9806G_SetPointPixel ( usX_Center - sCountY,   usY_Center - sCurrentX );           //4
				ILI9806G_SetPointPixel ( usX_Center - sCurrentX, usY_Center - sCountY );           //5    
				ILI9806G_SetPointPixel ( usX_Center + sCurrentX, usY_Center - sCountY );           //6
				ILI9806G_SetPointPixel ( usX_Center + sCountY,   usY_Center - sCurrentX );           //7 	
				ILI9806G_SetPointPixel ( usX_Center + sCountY,   usY_Center + sCurrentX );           //0				
			}
		
		else
		{          
			ILI9806G_SetPointPixel ( usX_Center + sCurrentX, usY_Center + sCurrentY );             //1，研究对象
			ILI9806G_SetPointPixel ( usX_Center - sCurrentX, usY_Center + sCurrentY );             //2      
			ILI9806G_SetPointPixel ( usX_Center - sCurrentY, usY_Center + sCurrentX );             //3
			ILI9806G_SetPointPixel ( usX_Center - sCurrentY, usY_Center - sCurrentX );             //4
			ILI9806G_SetPointPixel ( usX_Center - sCurrentX, usY_Center - sCurrentY );             //5       
			ILI9806G_SetPointPixel ( usX_Center + sCurrentX, usY_Center - sCurrentY );             //6
			ILI9806G_SetPointPixel ( usX_Center + sCurrentY, usY_Center - sCurrentX );             //7 
			ILI9806G_SetPointPixel ( usX_Center + sCurrentY, usY_Center + sCurrentX );             //0
		}			
		
		sCurrentX ++;

		
		if ( sError < 0 ) 
			sError += 4 * sCurrentX + 6;	  
		
		else
		{
			sError += 10 + 4 * ( sCurrentX - sCurrentY );   
			sCurrentY --;
		} 	
	}
}

/**
 * @brief  在 ILI9806G 显示器上显示一个英文字符
 * @param  usX ：在特定扫描方向下字符的起始X坐标
 * @param  usY ：在特定扫描方向下该点的起始Y坐标
 * @param  cChar ：要显示的英文字符
 * @note 可使用LCD_SetBackColor、LCD_SetTextColor、LCD_SetColors函数设置颜色
 * @retval 无
 */
void ILI9806G_DispChar_EN ( uint16_t usX, uint16_t usY, const char cChar )
{
	uint8_t  byteCount, bitCount,fontLength;	
	uint16_t ucRelativePositon;
	uint8_t *Pfont;
	
	//对ascii码表偏移（字模表不包含ASCII表的前32个非图形符号）
	ucRelativePositon = cChar - ' ';
	
	//每个字模的字节数
	fontLength = (LCD_Currentfonts->Width*LCD_Currentfonts->Height)/8;
		
	//字模首地址
	/*ascii码表偏移值乘以每个字模的字节数，求出字模的偏移位置*/
	Pfont = (uint8_t *)&LCD_Currentfonts->table[ucRelativePositon * fontLength];
	
	//设置显示窗口
	ILI9806G_OpenWindow ( usX, usY, LCD_Currentfonts->Width, LCD_Currentfonts->Height);
	
	ILI9806G_Write_Cmd ( CMD_SetPixel );			

	//按字节读取字模数据
	//由于前面直接设置了显示窗口，显示数据会自动换行
	for ( byteCount = 0; byteCount < fontLength; byteCount++ )
	{
		//一位一位处理要显示的颜色
		for ( bitCount = 0; bitCount < 8; bitCount++ )
		{
			if ( Pfont[byteCount] & (0x80>>bitCount) )
				ILI9806G_Write_Data ( CurrentTextColor );			
			else
				ILI9806G_Write_Data ( CurrentBackColor );
		}	
	}	
}


/**
 * @brief  在 ILI9806G 显示器上显示英文字符串
 * @param  line ：在特定扫描方向下字符串的起始Y坐标
  *   本参数可使用宏LINE(0)、LINE(1)等方式指定文字坐标，
  *   宏LINE(x)会根据当前选择的字体来计算Y坐标值。
	*		显示中文且使用LINE宏时，需要把英文字体设置成Font8x16
 * @param  pStr ：要显示的英文字符串的首地址
 * @note 可使用LCD_SetBackColor、LCD_SetTextColor、LCD_SetColors函数设置颜色
 * @retval 无
 */
void ILI9806G_DispStringLine_EN (  uint16_t line,  char * pStr )
{
	uint16_t usX = 0;
	
	while ( * pStr != '\0' )
	{
		if ( ( usX - ILI9806G_DispWindow_X_Star + LCD_Currentfonts->Width ) > LCD_X_LENGTH )
		{
			usX = ILI9806G_DispWindow_X_Star;
			line += LCD_Currentfonts->Height;
		}
		
		if ( ( line - ILI9806G_DispWindow_Y_Star + LCD_Currentfonts->Height ) > LCD_Y_LENGTH )
		{
			usX = ILI9806G_DispWindow_X_Star;
			line = ILI9806G_DispWindow_Y_Star;
		}
		
		ILI9806G_DispChar_EN ( usX, line, * pStr);
		
		pStr ++;
		
		usX += LCD_Currentfonts->Width;
		
	}
	
}


/**
 * @brief  在 ILI9806G 显示器上显示英文字符串
 * @param  usX ：在特定扫描方向下字符的起始X坐标
 * @param  usY ：在特定扫描方向下字符的起始Y坐标
 * @param  pStr ：要显示的英文字符串的首地址
 * @note 可使用LCD_SetBackColor、LCD_SetTextColor、LCD_SetColors函数设置颜色
 * @retval 无
 */
void ILI9806G_DispString_EN ( 	uint16_t usX ,uint16_t usY,  char * pStr )
{
	while ( * pStr != '\0' )
	{
		if ( ( usX - ILI9806G_DispWindow_X_Star + LCD_Currentfonts->Width ) > LCD_X_LENGTH )
		{
			usX = ILI9806G_DispWindow_X_Star;
			usY += LCD_Currentfonts->Height;
		}
		
		if ( ( usY - ILI9806G_DispWindow_Y_Star + LCD_Currentfonts->Height ) > LCD_Y_LENGTH )
		{
			usX = ILI9806G_DispWindow_X_Star;
			usY = ILI9806G_DispWindow_Y_Star;
		}
		
		ILI9806G_DispChar_EN ( usX, usY, * pStr);
		
		pStr ++;
		
		usX += LCD_Currentfonts->Width;	
	}	
}

/**
 * @brief  在 ILI9806G 显示器上显示英文字符串(沿Y轴方向)
 * @param  usX ：在特定扫描方向下字符的起始X坐标
 * @param  usY ：在特定扫描方向下字符的起始Y坐标
 * @param  pStr ：要显示的英文字符串的首地址
 * @note 可使用LCD_SetBackColor、LCD_SetTextColor、LCD_SetColors函数设置颜色
 * @retval 无
 */
void ILI9806G_DispString_EN_YDir (	 uint16_t usX,uint16_t usY ,  char * pStr )
{	
	while ( * pStr != '\0' )
	{
		if ( ( usY - ILI9806G_DispWindow_Y_Star + LCD_Currentfonts->Height ) >LCD_Y_LENGTH  )
		{
			usY = ILI9806G_DispWindow_Y_Star;
			usX += LCD_Currentfonts->Width;
		}
		
		if ( ( usX - ILI9806G_DispWindow_X_Star + LCD_Currentfonts->Width ) >  LCD_X_LENGTH)
		{
			usX = ILI9806G_DispWindow_X_Star;
			usY = ILI9806G_DispWindow_Y_Star;
		}
		
		ILI9806G_DispChar_EN ( usX, usY, * pStr);
		
		pStr ++;
		
		usY += LCD_Currentfonts->Height;		
	}	
}



/**
 * @brief  在 ILI9806G 显示器上显示一个中文字符
 * @param  usX ：在特定扫描方向下字符的起始X坐标
 * @param  usY ：在特定扫描方向下字符的起始Y坐标
 * @param  usChar ：要显示的中文字符（国标码）
 * @note 可使用LCD_SetBackColor、LCD_SetTextColor、LCD_SetColors函数设置颜色
 * @retval 无
 */ 
void ILI9806G_DispChar_CH ( uint16_t usX, uint16_t usY, uint16_t usChar )
{
	uint8_t rowCount, bitCount;
	uint32_t usTemp; 
	
	//	占用空间太大，改成全局变量 
	//	uint8_t ucBuffer [ WIDTH_CH_CHAR*HEIGHT_CH_CHAR/8 ];	
	
	//设置显示窗口
	ILI9806G_OpenWindow ( usX, usY, WIDTH_CH_CHAR, HEIGHT_CH_CHAR );
	
	ILI9806G_Write_Cmd ( CMD_SetPixel );
	
	//取字模数据  
	GetGBKCode ( ucBuffer, usChar );	
	
	for ( rowCount = 0; rowCount < HEIGHT_CH_CHAR; rowCount++ )
	{
    /* 取出四个字节的数据，在lcd上即是一个汉字的一行 */
		usTemp = ucBuffer [ rowCount * 4 ];
		usTemp = ( usTemp << 8 );
		usTemp |= ucBuffer [ rowCount * 4 + 1 ];
		usTemp = ( usTemp << 8 );
		usTemp |= ucBuffer [ rowCount * 4 + 2 ];
		usTemp = ( usTemp << 8 );
		usTemp |= ucBuffer [ rowCount * 4 + 3 ];
		
		for ( bitCount = 0; bitCount < WIDTH_CH_CHAR; bitCount ++ )
		{			
			if ( usTemp & ( 0x80000000 >> bitCount ) )  //高位在前 
			  ILI9806G_Write_Data ( CurrentTextColor );				
			else
				ILI9806G_Write_Data ( CurrentBackColor );			
		}		
	}
	
}


/**
 * @brief  在 ILI9806G 显示器上显示中文字符串
 * @param  line ：在特定扫描方向下字符串的起始Y坐标
  *   本参数可使用宏LINE(0)、LINE(1)等方式指定文字坐标，
  *   宏LINE(x)会根据当前选择的字体来计算Y坐标值。
	*		显示中文且使用LINE宏时，需要把英文字体设置成Font8x16
 * @param  pStr ：要显示的英文字符串的首地址
 * @note 可使用LCD_SetBackColor、LCD_SetTextColor、LCD_SetColors函数设置颜色
 * @retval 无
 */
void ILI9806G_DispString_CH ( 	uint16_t usX , uint16_t usY, char * pStr )
{	
	uint16_t usCh;

	
	while( * pStr != '\0' )
	{		
		if ( ( usX - ILI9806G_DispWindow_X_Star + WIDTH_CH_CHAR ) > LCD_X_LENGTH )
		{
			usX = ILI9806G_DispWindow_X_Star;
			usY += HEIGHT_CH_CHAR;
		}
		
		if ( ( usY - ILI9806G_DispWindow_Y_Star + HEIGHT_CH_CHAR ) > LCD_Y_LENGTH )
		{
			usX = ILI9806G_DispWindow_X_Star;
			usY = ILI9806G_DispWindow_Y_Star;
		}	
		
		usCh = * ( uint16_t * ) pStr;	
	  usCh = ( usCh << 8 ) + ( usCh >> 8 );

		ILI9806G_DispChar_CH ( usX, usY, usCh );
		
		usX += WIDTH_CH_CHAR;
		
		pStr += 2;           //一个汉字两个字节 

	}	   
	
}


/**
 * @brief  在 ILI9806G 显示器上显示中英文字符串
 * @param  line ：在特定扫描方向下字符串的起始Y坐标
  *   本参数可使用宏LINE(0)、LINE(1)等方式指定文字坐标，
  *   宏LINE(x)会根据当前选择的字体来计算Y坐标值。
	*		显示中文且使用LINE宏时，需要把英文字体设置成Font8x16
 * @param  pStr ：要显示的字符串的首地址
 * @note 可使用LCD_SetBackColor、LCD_SetTextColor、LCD_SetColors函数设置颜色
 * @retval 无
 */
void ILI9806G_DispStringLine_EN_CH (  uint16_t line, char * pStr )
{
	uint16_t usCh;
	uint16_t usX = 0;
	
	while( * pStr != '\0' )
	{
		if ( * pStr <= 126 )	           	//英文字符
		{
			if ( ( usX - ILI9806G_DispWindow_X_Star + LCD_Currentfonts->Width ) > LCD_X_LENGTH )
			{
				usX = ILI9806G_DispWindow_X_Star;
				line += LCD_Currentfonts->Height;
			}
			
			if ( ( line - ILI9806G_DispWindow_Y_Star + LCD_Currentfonts->Height ) > LCD_Y_LENGTH )
			{
				usX = ILI9806G_DispWindow_X_Star;
				line = ILI9806G_DispWindow_Y_Star;
			}			
		
		  ILI9806G_DispChar_EN ( usX, line, * pStr );
			
			usX +=  LCD_Currentfonts->Width;
		
		  pStr ++;

		}
		
		else	                            //汉字字符
		{
			if ( ( usX - ILI9806G_DispWindow_X_Star + WIDTH_CH_CHAR ) > LCD_X_LENGTH )
			{
				usX = ILI9806G_DispWindow_X_Star;
				line += HEIGHT_CH_CHAR;
			}
			
			if ( ( line - ILI9806G_DispWindow_Y_Star + HEIGHT_CH_CHAR ) > LCD_Y_LENGTH )
			{
				usX = ILI9806G_DispWindow_X_Star;
				line = ILI9806G_DispWindow_Y_Star;
			}	
			
			usCh = * ( uint16_t * ) pStr;	
			
			usCh = ( usCh << 8 ) + ( usCh >> 8 );		

			ILI9806G_DispChar_CH ( usX, line, usCh );
			
			usX += WIDTH_CH_CHAR;
			
			pStr += 2;           //一个汉字两个字节 
		
    }
		
  }	
} 

/**
 * @brief  在 ILI9806G 显示器上显示中英文字符串
 * @param  usX ：在特定扫描方向下字符的起始X坐标
 * @param  usY ：在特定扫描方向下字符的起始Y坐标
 * @param  pStr ：要显示的字符串的首地址
 * @note 可使用LCD_SetBackColor、LCD_SetTextColor、LCD_SetColors函数设置颜色
 * @retval 无
 */
void ILI9806G_DispString_EN_CH ( 	uint16_t usX , uint16_t usY, char * pStr )
{
	uint16_t usCh;
	uint8_t firstByte;
	
	while( * pStr != '\0' )
	{
		firstByte = (uint8_t)*pStr;
		if ( firstByte <= 126U )	           	//英文字符
		{
			if ( ( usX - ILI9806G_DispWindow_X_Star + LCD_Currentfonts->Width ) > LCD_X_LENGTH )
			{
				usX = ILI9806G_DispWindow_X_Star;
				usY += LCD_Currentfonts->Height;
			}
			
			if ( ( usY - ILI9806G_DispWindow_Y_Star + LCD_Currentfonts->Height ) > LCD_Y_LENGTH )
			{
				usX = ILI9806G_DispWindow_X_Star;
				usY = ILI9806G_DispWindow_Y_Star;
			}			
		
		  ILI9806G_DispChar_EN ( usX, usY, (char)firstByte );
			
			usX +=  LCD_Currentfonts->Width;
		
		  pStr ++;

		}
		
		else if ( pStr[1] != '\0' )	       //汉字字符
		{
			if ( ( usX - ILI9806G_DispWindow_X_Star + WIDTH_CH_CHAR ) > LCD_X_LENGTH )
			{
				usX = ILI9806G_DispWindow_X_Star;
				usY += HEIGHT_CH_CHAR;
			}
			
			if ( ( usY - ILI9806G_DispWindow_Y_Star + HEIGHT_CH_CHAR ) > LCD_Y_LENGTH )
			{
				usX = ILI9806G_DispWindow_X_Star;
				usY = ILI9806G_DispWindow_Y_Star;
			}	
			
			/* Avoid an unaligned uint16_t load after an odd-length ASCII prefix. */
			usCh = ((uint16_t)firstByte << 8) | (uint8_t)pStr[1];

			ILI9806G_DispChar_CH ( usX, usY, usCh );
			
			usX += WIDTH_CH_CHAR;
			
			pStr += 2;           //一个汉字两个字节 
		
    }
		else
		{
			ILI9806G_DispChar_EN ( usX, usY, '?' );
			usX += LCD_Currentfonts->Width;
			pStr++;
		}
		
  }	
} 

/**
 * @brief  在 ILI9806G 显示器上显示中英文字符串(沿Y轴方向)
 * @param  usX ：在特定扫描方向下字符的起始X坐标
 * @param  usY ：在特定扫描方向下字符的起始Y坐标
 * @param  pStr ：要显示的中英文字符串的首地址
 * @note 可使用LCD_SetBackColor、LCD_SetTextColor、LCD_SetColors函数设置颜色
 * @retval 无
 */
void ILI9806G_DispString_EN_CH_YDir (  uint16_t usX,uint16_t usY , char * pStr )
{
	uint16_t usCh;
	
	while( * pStr != '\0' )
	{			
			//统一使用汉字的宽高来计算换行
			if ( ( usY - ILI9806G_DispWindow_Y_Star + HEIGHT_CH_CHAR ) >LCD_Y_LENGTH  )
			{
				usY = ILI9806G_DispWindow_Y_Star;
				usX += WIDTH_CH_CHAR;
			}			
			if ( ( usX - ILI9806G_DispWindow_X_Star + WIDTH_CH_CHAR ) >  LCD_X_LENGTH)
			{
				usX = ILI9806G_DispWindow_X_Star;
				usY = ILI9806G_DispWindow_Y_Star;
			}
			
		//显示	
		if ( * pStr <= 126 )	           	//英文字符
		{			
			ILI9806G_DispChar_EN ( usX, usY, * pStr);
			
			pStr ++;
			
			usY += HEIGHT_CH_CHAR;		
		}
		else	                            //汉字字符
		{			
			usCh = * ( uint16_t * ) pStr;	
			
			usCh = ( usCh << 8 ) + ( usCh >> 8 );		

			ILI9806G_DispChar_CH ( usX,usY , usCh );
			
			usY += HEIGHT_CH_CHAR;
			
			pStr += 2;           //一个汉字两个字节 
		
    }
		
  }	
} 

/***********************缩放字体****************************/
#define ZOOMMAXBUFF 16384
uint8_t zoomBuff[ZOOMMAXBUFF] = {0};	//用于缩放的缓存，最大支持到128*128
uint8_t zoomTempBuff[1024] = {0};

/**
 * @brief  缩放字模，缩放后的字模由1个像素点由8个数据位来表示
										0x01表示笔迹，0x00表示空白区
 * @param  in_width ：原始字符宽度
 * @param  in_heig ：原始字符高度
 * @param  out_width ：缩放后的字符宽度
 * @param  out_heig：缩放后的字符高度
 * @param  in_ptr ：字库输入指针	注意：1pixel 1bit
 * @param  out_ptr ：缩放后的字符输出指针 注意: 1pixel 8bit
 *		out_ptr实际上没有正常输出，改成了直接输出到全局指针zoomBuff中
 * @param  en_cn ：0为英文，1为中文
 * @retval 无
 */
void ILI9806G_zoomChar(uint16_t in_width,	//原始字符宽度
									uint16_t in_heig,		//原始字符高度
									uint16_t out_width,	//缩放后的字符宽度
									uint16_t out_heig,	//缩放后的字符高度
									uint8_t *in_ptr,	//字库输入指针	注意：1pixel 1bit
									uint8_t *out_ptr, //缩放后的字符输出指针 注意: 1pixel 8bit
									uint8_t en_cn)		//0为英文，1为中文	
{
	uint8_t *pts,*ots;
	//根据源字模及目标字模大小，设定运算比例因子，左移16是为了把浮点运算转成定点运算
	unsigned int xrIntFloat_16=(in_width<<16)/out_width+1; 
  unsigned int yrIntFloat_16=(in_heig<<16)/out_heig+1;
	
	unsigned int srcy_16=0;
	unsigned int y,x;
	uint8_t *pSrcLine;
	
	uint16_t byteCount,bitCount;
	
	//检查参数是否合法
	if(in_width > 48) return;												//字库不允许超过48像素
	if(in_width * in_heig == 0) return;	
	if(in_width * in_heig > 48*48 ) return; 					//限制输入最大 48*48
	
	if(out_width * out_heig == 0) return;	
	if(out_width * out_heig >= ZOOMMAXBUFF ) return; //限制最大缩放 128*128
	pts = (uint8_t*)&zoomTempBuff;
	
	//为方便运算，字库的数据由1 pixel/1bit 映射到1pixel/8bit
	//0x01表示笔迹，0x00表示空白区
	if(en_cn == 0x00)//英文
	{
		//英文和中文字库上下边界不对，可在此处调整。需要注意tempBuff防止溢出
			for(byteCount=0;byteCount<in_heig*in_width/8;byteCount++)	
			{
				for(bitCount=0;bitCount<8;bitCount++)
					{						
						//把源字模数据由位映射到字节
						//in_ptr里bitX为1，则pts里整个字节值为1
						//in_ptr里bitX为0，则pts里整个字节值为0
						*pts++ = (in_ptr[byteCount] & (0x80>>bitCount))?1:0; 
					}
			}				
	}
	else //中文
	{			
			for(byteCount=0;byteCount<in_heig*in_width/8;byteCount++)	
			{
				for(bitCount=0;bitCount<8;bitCount++)
					{						
						//把源字模数据由位映射到字节
						//in_ptr里bitX为1，则pts里整个字节值为1
						//in_ptr里bitX为0，则pts里整个字节值为0
						*pts++ = (in_ptr[byteCount] & (0x80>>bitCount))?1:0; 
					}
			}		
	}

	//zoom过程
	pts = (uint8_t*)&zoomTempBuff;	//映射后的源数据指针
	ots = (uint8_t*)&zoomBuff;	//输出数据的指针
	for (y=0;y<out_heig;y++)	/*行遍历*/
    {
				unsigned int srcx_16=0;
        pSrcLine=pts+in_width*(srcy_16>>16);				
        for (x=0;x<out_width;x++) /*行内像素遍历*/
        {
            ots[x]=pSrcLine[srcx_16>>16]; //把源字模数据复制到目标指针中
            srcx_16+=xrIntFloat_16;			//按比例偏移源像素点
        }
        srcy_16+=yrIntFloat_16;				  //按比例偏移源像素点
        ots+=out_width;						
    }
	/*！！！缩放后的字模数据直接存储到全局指针zoomBuff里了*/
	out_ptr = (uint8_t*)&zoomBuff;	//out_ptr没有正确传出，后面调用直接改成了全局变量指针！
	
	/*实际中如果使用out_ptr不需要下面这一句！！！
		只是因为out_ptr没有使用，会导致warning。强迫症*/
	out_ptr++; 
}			


/**
 * @brief  利用缩放后的字模显示字符
 * @param  Xpos ：字符显示位置x
 * @param  Ypos ：字符显示位置y
 * @param  Font_width ：字符宽度
 * @param  Font_Heig：字符高度
 * @param  c ：要显示的字模数据
 * @param  DrawModel ：是否反色显示 
 * @retval 无
 */
void ILI9806G_DrawChar_Ex(uint16_t usX, //字符显示位置x
												uint16_t usY, //字符显示位置y
												uint16_t Font_width, //字符宽度
												uint16_t Font_Height,  //字符高度 
												uint8_t *c,						//字模数据
												uint16_t DrawModel)		//是否反色显示
{
  uint32_t index = 0, counter = 0;

	//设置显示窗口
	ILI9806G_OpenWindow ( usX, usY, Font_width, Font_Height);
	
	ILI9806G_Write_Cmd ( CMD_SetPixel );		
	
	//按字节读取字模数据
	//由于前面直接设置了显示窗口，显示数据会自动换行
	for ( index = 0; index < Font_Height; index++ )
	{
			//一位一位处理要显示的颜色
			for ( counter = 0; counter < Font_width; counter++ )
			{
					//缩放后的字模数据，以一个字节表示一个像素位
					//整个字节值为1表示该像素为笔迹
					//整个字节值为0表示该像素为背景
					if ( *c++ == DrawModel )
						ILI9806G_Write_Data ( CurrentBackColor );			
					else
						ILI9806G_Write_Data ( CurrentTextColor );
			}	
	}	
}


/**
 * @brief  利用缩放后的字模显示字符串
 * @param  Xpos ：字符显示位置x
 * @param  Ypos ：字符显示位置y
 * @param  Font_width ：字符宽度，英文字符在此基础上/2。注意为偶数
 * @param  Font_Heig：字符高度，注意为偶数
 * @param  c ：要显示的字符串
 * @param  DrawModel ：是否反色显示 
 * @retval 无
 */
void ILI9806G_DisplayStringEx(uint16_t x, 		//字符显示位置x
														 uint16_t y, 				//字符显示位置y
														 uint16_t Font_width,	//要显示的字体宽度，英文字符在此基础上/2。注意为偶数
														 uint16_t Font_Height,	//要显示的字体高度，注意为偶数
														 uint8_t *ptr,					//显示的字符内容
														 uint16_t DrawModel)  //是否反色显示



{
	uint16_t Charwidth = Font_width; //默认为Font_width，英文宽度为中文宽度的一半
	uint8_t *psr;
	uint8_t Ascii;	//英文
	uint16_t usCh;  //中文
	
	//占用空间太大，改成全局变量	
	//	uint8_t ucBuffer [ WIDTH_CH_CHAR*HEIGHT_CH_CHAR/8 ];	
	
	while ( *ptr != '\0')
	{
			/****处理换行*****/
			if ( ( x - ILI9806G_DispWindow_X_Star + Charwidth ) > LCD_X_LENGTH )
			{
				x = ILI9806G_DispWindow_X_Star;
				y += Font_Height;
			}
			
			if ( ( y - ILI9806G_DispWindow_Y_Star + Font_Height ) > LCD_Y_LENGTH )
			{
				x = ILI9806G_DispWindow_X_Star;
				y = ILI9806G_DispWindow_Y_Star;
			}	
			
		if(*ptr > 0x80) //如果是中文
		{			
			Charwidth = Font_width;
			usCh = * ( uint16_t * ) ptr;				
			usCh = ( usCh << 8 ) + ( usCh >> 8 );
			GetGBKCode ( ucBuffer, usCh );	//取字模数据
			//缩放字模数据，源字模为32*32
			ILI9806G_zoomChar(WIDTH_CH_CHAR,HEIGHT_CH_CHAR,Charwidth,Font_Height,(uint8_t *)&ucBuffer,psr,1); 
			//显示单个字符
			ILI9806G_DrawChar_Ex(x,y,Charwidth,Font_Height,(uint8_t*)&zoomBuff,DrawModel);
			x+=Charwidth;
			ptr+=2;
		}
		else
		{
				Charwidth = Font_width / 2;
				Ascii = *ptr - 32;
				//使用16*32字体缩放字模数据
				ILI9806G_zoomChar(16,32,Charwidth,Font_Height,(uint8_t *)&Font16x32.table[Ascii * Font16x32.Height*Font16x32.Width/8],psr,0);
			  //显示单个字符
				ILI9806G_DrawChar_Ex(x,y,Charwidth,Font_Height,(uint8_t*)&zoomBuff,DrawModel);
				x+=Charwidth;
				ptr++;
		}
	}
}


/**
 * @brief  利用缩放后的字模显示字符串(沿Y轴方向)
 * @param  Xpos ：字符显示位置x
 * @param  Ypos ：字符显示位置y
 * @param  Font_width ：字符宽度，英文字符在此基础上/2。注意为偶数
 * @param  Font_Heig：字符高度，注意为偶数
 * @param  c ：要显示的字符串
 * @param  DrawModel ：是否反色显示 
 * @retval 无
 */
void ILI9806G_DisplayStringEx_YDir(uint16_t x, 		//字符显示位置x
																		 uint16_t y, 				//字符显示位置y
																		 uint16_t Font_width,	//要显示的字体宽度，英文字符在此基础上/2。注意为偶数
																		 uint16_t Font_Height,	//要显示的字体高度，注意为偶数
																		 uint8_t *ptr,					//显示的字符内容
																		 uint16_t DrawModel)  //是否反色显示
{
	uint16_t Charwidth = Font_width; //默认为Font_width，英文宽度为中文宽度的一半
	uint8_t *psr;
	uint8_t Ascii;	//英文
	uint16_t usCh;  //中文
	uint8_t ucBuffer [ WIDTH_CH_CHAR*HEIGHT_CH_CHAR/8 ];	
	
	while ( *ptr != '\0')
	{			
			//统一使用汉字的宽高来计算换行
			if ( ( y - ILI9806G_DispWindow_X_Star + Font_width ) > LCD_X_LENGTH )
			{
				y = ILI9806G_DispWindow_X_Star;
				x += Font_width;
			}
			
			if ( ( x - ILI9806G_DispWindow_Y_Star + Font_Height ) > LCD_Y_LENGTH )
			{
				y = ILI9806G_DispWindow_X_Star;
				x = ILI9806G_DispWindow_Y_Star;
			}	
			
		if(*ptr > 0x80) //如果是中文
		{			
			Charwidth = Font_width;
			usCh = * ( uint16_t * ) ptr;				
			usCh = ( usCh << 8 ) + ( usCh >> 8 );
			GetGBKCode ( ucBuffer, usCh );	//取字模数据
			//缩放字模数据，源字模为16*16
			ILI9806G_zoomChar(WIDTH_CH_CHAR,HEIGHT_CH_CHAR,Charwidth,Font_Height,(uint8_t *)&ucBuffer,psr,1); 
			//显示单个字符
			ILI9806G_DrawChar_Ex(x,y,Charwidth,Font_Height,(uint8_t*)&zoomBuff,DrawModel);
			y+=Font_Height;
			ptr+=2;
		}
		else
		{
				Charwidth = Font_width / 2;
				Ascii = *ptr - 32;
				//使用16*24字体缩放字模数据
				ILI9806G_zoomChar(16,24,Charwidth,Font_Height,(uint8_t *)&Font16x32.table[Ascii * Font16x32.Height*Font16x32.Width/8],psr,0);
			  //显示单个字符
				ILI9806G_DrawChar_Ex(x,y,Charwidth,Font_Height,(uint8_t*)&zoomBuff,DrawModel);
				y+=Font_Height;
				ptr++;
		}
	}
}

/**
  * @brief  设置英文字体类型
  * @param  fonts: 指定要选择的字体
	*		参数为以下值之一
  * 	@arg：Font24x32;
  * 	@arg：Font16x24;
  * 	@arg：Font8x16;
  * @retval None
  */
void LCD_SetFont(sFONT *fonts)
{
	LCD_Currentfonts = fonts;
}

/**
  * @brief  获取当前字体类型
  * @param  None.
  * @retval 返回当前字体类型
  */
sFONT *LCD_GetFont(void)
{
	return LCD_Currentfonts;
}


/**
  * @brief  设置LCD的前景(字体)及背景颜色,RGB565
  * @param  TextColor: 指定前景(字体)颜色
  * @param  BackColor: 指定背景颜色
  * @retval None
  */
void LCD_SetColors(uint16_t TextColor, uint16_t BackColor) 
{
	CurrentTextColor = TextColor;
	CurrentBackColor = BackColor;
}

/**
  * @brief  获取LCD的前景(字体)及背景颜色,RGB565
  * @param  TextColor: 用来存储前景(字体)颜色的指针变量
  * @param  BackColor: 用来存储背景颜色的指针变量
  * @retval None
  */
void LCD_GetColors(uint16_t *TextColor, uint16_t *BackColor)
{
	*TextColor = CurrentTextColor;
	*BackColor = CurrentBackColor;
}

/**
  * @brief  设置LCD的前景(字体)颜色,RGB565
  * @param  Color: 指定前景(字体)颜色 
  * @retval None
  */
void LCD_SetTextColor(uint16_t Color)
{
	CurrentTextColor = Color;
}

/**
  * @brief  设置LCD的背景颜色,RGB565
  * @param  Color: 指定背景颜色 
  * @retval None
  */
void LCD_SetBackColor(uint16_t Color)
{
	CurrentBackColor = Color;
}

/**
  * @brief  清除某行文字
  * @param  Line: 指定要删除的行
  *   本参数可使用宏LINE(0)、LINE(1)等方式指定要删除的行，
  *   宏LINE(x)会根据当前选择的字体来计算Y坐标值，并删除当前字体高度的第x行。
  * @retval None
  */
void ILI9806G_ClearLine(uint16_t Line)
{
	ILI9806G_Clear(0,Line,LCD_X_LENGTH,((sFONT *)LCD_GetFont())->Height);	/* 清屏，显示全黑 */
}




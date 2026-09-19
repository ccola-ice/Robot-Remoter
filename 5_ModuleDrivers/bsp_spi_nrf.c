#include "bsp_spi_nrf.h"
#include "bsp_usart_debug.h"
#include "bsp_SysTick.h"

u8 RX_BUF[RX_PLOAD_WIDTH];		//接收数据缓存
u8 TX_BUF[TX_PLOAD_WIDTH];		//发射数据缓存
u8 TX_ADDRESS[TX_ADR_WIDTH] = {0x34,0x43,0x10,0x10,0x01};  // 定义一个静态发送地址
u8 RX_ADDRESS[RX_ADR_WIDTH] = {0x34,0x43,0x10,0x10,0x01};
static u8 nrf_channel = CHANAL;
static u8 nrf_rf_setup = 0x0f;
static uint8_t nrf_io_error;
static uint8_t nrf_tx_active;
static uint8_t nrf_tx_enabled;
static uint32_t nrf_tx_started;
static uint32_t nrf_config_generation;

uint32_t NRF_GetConfigGeneration(void) { return nrf_config_generation; }

uint8_t NRF_GetIoError(void)
{
    return nrf_io_error;
}

static void NRF_IoFault(uint8_t error)
{
    if(nrf_io_error == 0U) nrf_io_error = error;
    nrf_tx_active = 0U;
    nrf_tx_enabled = 0U;
    NRF_CE_LOW();
}

static void NRF_ResetState(void)
{
    nrf_io_error = 0U;
    nrf_tx_active = 0U;
    nrf_tx_enabled = 0U;
    nrf_tx_started = 0U;
    NRF_CE_LOW();
}

void Delay(__IO u32 nCount)
{
  for(; nCount != 0; nCount--);
} 

/**
  * @brief  SPI的 I/O配置
  * @param  无
  * @retval 无
  */
void NRF_SPI_Init(void)
{
  SPI_InitTypeDef  SPI_InitStructure;
  GPIO_InitTypeDef GPIO_InitStructure;
  
  RCC_AHB1PeriphClockCmd (NRF_SPI_SCK_GPIO_CLK | NRF_SPI_MISO_GPIO_CLK|NRF_SPI_MOSI_GPIO_CLK, ENABLE);

  /*开启相应IO端口的时钟*/
  RCC_AHB1PeriphClockCmd(NRF_CSN_GPIO_CLK
                         |NRF_CE_GPIO_CLK
                         |NRF_IRQ_GPIO_CLK,ENABLE);

  /*配置SPI_NRF_SPI的CE引脚,和SPI_NRF_SPI的 CSN 引脚*/														   
  GPIO_InitStructure.GPIO_Pin = NRF_CSN_PIN;	
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;  
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz; 
  GPIO_Init(NRF_CSN_GPIO_PORT, &GPIO_InitStructure);
  
  GPIO_InitStructure.GPIO_Pin = NRF_CE_PIN;	
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;  
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz; 
  GPIO_Init(NRF_CE_GPIO_PORT, &GPIO_InitStructure);
  
  GPIO_InitStructure.GPIO_Pin = NRF_IRQ_PIN;	
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;  
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz; 
  GPIO_Init(NRF_IRQ_GPIO_PORT, &GPIO_InitStructure);
  
  /*!< SPI_NRF_SPI 时钟使能 */
  NRF_SPI_CLK_INIT(NRF_SPI_CLK, ENABLE);
 
  //设置引脚复用
  GPIO_PinAFConfig(NRF_SPI_SCK_GPIO_PORT,NRF_SPI_SCK_PINSOURCE,NRF_SPI_SCK_AF); 
  GPIO_PinAFConfig(NRF_SPI_MISO_GPIO_PORT,NRF_SPI_MISO_PINSOURCE,NRF_SPI_MISO_AF); 
  GPIO_PinAFConfig(NRF_SPI_MOSI_GPIO_PORT,NRF_SPI_MOSI_PINSOURCE,NRF_SPI_MOSI_AF); 
  
  /*!< 配置 SPI_NRF_SPI 引脚: SCK */
  GPIO_InitStructure.GPIO_Pin = NRF_SPI_SCK_PIN;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;  
  
  GPIO_Init(NRF_SPI_SCK_GPIO_PORT, &GPIO_InitStructure);
  
  /*!< 配置 SPI_NRF_SPI 引脚: MISO */
  GPIO_InitStructure.GPIO_Pin = NRF_SPI_MISO_PIN;
  GPIO_Init(NRF_SPI_MISO_GPIO_PORT, &GPIO_InitStructure);
  
  /*!< 配置 SPI_NRF_SPI 引脚: MOSI */
  GPIO_InitStructure.GPIO_Pin = NRF_SPI_MOSI_PIN;
  GPIO_Init(NRF_SPI_MOSI_GPIO_PORT, &GPIO_InitStructure);  

    nrf_config_generation++;

  NRF_ResetState();

  /* Keep the radio deselected and out of RX/TX while SPI is configured. */
  NRF_CE_LOW();
  NRF_CSN_HIGH();

  /* NRF_SPI 模式配置 */
  // NRF芯片 支持SPI模式0及模式3，据此设置CPOL CPHA
  SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex; //双线全双工
  SPI_InitStructure.SPI_Mode = SPI_Mode_Master;	 					           //主模式
  SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;	 				         //数据大小8位
  SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;		 			               //时钟极性，空闲时为低
  SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;						           //第1个边沿有效，上升沿为采样时刻
  SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;		   					           //NSS信号由软件产生
  SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_16; //16分频，MHz
  SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;  				       //高位在前
  SPI_InitStructure.SPI_CRCPolynomial = 7;

  SPI_Init(NRF_SPI, &SPI_InitStructure);

  /* 使能 NRF_SPI  */
  SPI_Cmd(NRF_SPI, ENABLE);
  NRF_PowerDown();
}

/**
  * @brief   用于向NRF读/写一字节数据
  * @param   写入的数据
  *		@arg dat 
  * @retval  读取得的数据
  */
u8 SPI_NRF_RW(u8 dat)
{
    uint32_t remaining = NRF_TIMEOUT;
    if(nrf_io_error != 0U) return 0xffU;
    while(SPI_I2S_GetFlagStatus(NRF_SPI, SPI_I2S_FLAG_TXE) == RESET) {
        if(remaining-- == 0U) { NRF_IoFault(1U); return 0xffU; }
    }
    SPI_I2S_SendData(NRF_SPI, dat);
    remaining = NRF_TIMEOUT;
    while(SPI_I2S_GetFlagStatus(NRF_SPI, SPI_I2S_FLAG_RXNE) == RESET) {
        if(remaining-- == 0U) { NRF_IoFault(2U); return 0xffU; }
    }
    return (uint8_t)SPI_I2S_ReceiveData(NRF_SPI);
}

/**
  * @brief   用于向NRF特定的寄存器写入数据
  * @param   
  *		@arg reg:NRF的命令+寄存器地址
  *		@arg dat:将要向寄存器写入的数据
  * @retval  NRF的status寄存器的状态
  */
u8 SPI_NRF_WriteReg(u8 reg,u8 dat)
{
 	u8 status;
	 NRF_CE_LOW();
	/*置低CSN，使能SPI传输*/
    NRF_CSN_LOW();
				
	/*发送命令及寄存器号 */
	status = SPI_NRF_RW(reg);
		 
	 /*向寄存器写入数据*/
    SPI_NRF_RW(dat);
	          
	/*CSN拉高，完成*/	   
  	NRF_CSN_HIGH();	
		
	/*返回状态寄存器的值*/
   	return(status);
}

/**
  * @brief   用于从NRF特定的寄存器读出数据
  * @param   
  *		@arg reg:NRF的命令+寄存器地址
  * @retval  寄存器中的数据
  */
u8 SPI_NRF_ReadReg(u8 reg)
{
 	u8 reg_val;

	NRF_CE_LOW();
	/*置低CSN，使能SPI传输*/
 	NRF_CSN_LOW();
				
  	 /*发送寄存器号*/
	SPI_NRF_RW(reg); 

	 /*读取寄存器的值 */
	reg_val = SPI_NRF_RW(NOP);
	            
   	/*CSN拉高，完成*/
	NRF_CSN_HIGH();		
   	
	return reg_val;
}	

/**
  * @brief   用于向NRF的寄存器中写入一串数据
  * @param   
  *		@arg reg : NRF的命令+寄存器地址
  *		@arg pBuf：用于存储将被读出的寄存器数据的数组，外部定义
  * 	@arg bytes: pBuf的数据长度
  * @retval  NRF的status寄存器的状态
  */
u8 SPI_NRF_ReadBuf(u8 reg,u8 *pBuf,u8 bytes)
{
 	u8 status, byte_cnt;
    if(pBuf == 0 || bytes > 32U) { NRF_IoFault(4U); return 0xffU; }


	  NRF_CE_LOW();
	/*置低CSN，使能SPI传输*/
	NRF_CSN_LOW();
		
	/*发送寄存器号*/		
	status = SPI_NRF_RW(reg); 

 	/*读取缓冲区数据*/
	 for(byte_cnt=0;byte_cnt<bytes;byte_cnt++)		  
	   pBuf[byte_cnt] = SPI_NRF_RW(NOP); //从NRF24L01读取数据  

	 /*CSN拉高，完成*/
	NRF_CSN_HIGH();	
		
 	return status;		//返回寄存器状态值
}

/**
  * @brief   用于向NRF的寄存器中写入一串数据
  * @param   
  *		@arg reg : NRF的命令+寄存器地址
  *		@arg pBuf：存储了将要写入写寄存器数据的数组，外部定义
  * 	@arg bytes: pBuf的数据长度
  * @retval  NRF的status寄存器的状态
  */
u8 SPI_NRF_WriteBuf(u8 reg ,u8 *pBuf,u8 bytes)
{
	 u8 status,byte_cnt;
    if(pBuf == 0 || bytes > 32U) { NRF_IoFault(4U); return 0xffU; }

	 NRF_CE_LOW();
   	 /*置低CSN，使能SPI传输*/
	 NRF_CSN_LOW();			

	 /*发送寄存器号*/	
  	 status = SPI_NRF_RW(reg); 
 	
  	  /*向缓冲区写入数据*/
	 for(byte_cnt=0;byte_cnt<bytes;byte_cnt++)
		SPI_NRF_RW(*pBuf++);	//写数据到缓冲区 	 
	  	   
	/*CSN拉高，完成*/
	NRF_CSN_HIGH();			
  
  	return (status);	//返回NRF24L01的状态 		
}

/**
  * @brief  配置并进入接收模式
  * @param  无
  * @retval 无
  */
void NRF_RX_Mode(void)
{
    SPI_NRF_WriteReg(NRF_WRITE_REG + CONFIG, 0x0cU);

    nrf_config_generation++;
    NRF_TxCancel();
    nrf_tx_enabled = 0U;
	NRF_CE_LOW();	

	SPI_NRF_WriteBuf(NRF_WRITE_REG+RX_ADDR_P0,RX_ADDRESS,RX_ADR_WIDTH);//写RX节点地址
	
	SPI_NRF_WriteReg(NRF_WRITE_REG+EN_AA,0x01);    //使能通道0的自动应答    
	
	SPI_NRF_WriteReg(NRF_WRITE_REG+EN_RXADDR,0x01);//使能通道0的接收地址    
	
	SPI_NRF_WriteReg(NRF_WRITE_REG+RF_CH,nrf_channel);      //设置RF通信频率
	
	SPI_NRF_WriteReg(NRF_WRITE_REG+RX_PW_P0,RX_PLOAD_WIDTH);//选择通道0的有效数据宽度      
	
	SPI_NRF_WriteReg(NRF_WRITE_REG+RF_SETUP,nrf_rf_setup); //设置TX发射功率和空中速率
	
	SPI_NRF_WriteReg(NRF_WRITE_REG+CONFIG, 0x0f);  //配置基本工作模式的参数;PWR_UP,EN_CRC,16BIT_CRC,接收模式 

	/*CE拉高，进入接收模式*/	
	if(nrf_io_error == 0U) { Delay_us(1600U); NRF_CE_HIGH(); }
}    

/**
  * @brief  配置发送模式
  * @param  无
  * @retval 无
  */
void NRF_TX_Mode(void)
{
    SPI_NRF_WriteReg(NRF_WRITE_REG + CONFIG, 0x0cU);

    nrf_config_generation++;
    NRF_TxCancel();
    nrf_tx_enabled = 0U;
	NRF_CE_LOW();		

	SPI_NRF_WriteBuf(NRF_WRITE_REG+TX_ADDR,TX_ADDRESS,TX_ADR_WIDTH);    //写TX节点地址 
	
	SPI_NRF_WriteBuf(NRF_WRITE_REG+RX_ADDR_P0,RX_ADDRESS,RX_ADR_WIDTH); //设置TX节点地址,主要为了使能ACK   
	
	SPI_NRF_WriteReg(NRF_WRITE_REG+EN_AA,0x01);     //使能通道0的自动应答    
	
	SPI_NRF_WriteReg(NRF_WRITE_REG+EN_RXADDR,0x01); //使能通道0的接收地址  
	
	SPI_NRF_WriteReg(NRF_WRITE_REG+SETUP_RETR,0x1a);//设置自动重发间隔时间:500us + 86us;最大自动重发次数:10次
	
	SPI_NRF_WriteReg(NRF_WRITE_REG+RF_CH,nrf_channel);       //设置RF通道
	
	SPI_NRF_WriteReg(NRF_WRITE_REG+RF_SETUP,nrf_rf_setup);  //设置TX发射功率和空中速率
		
	SPI_NRF_WriteReg(NRF_WRITE_REG+CONFIG,0x0e);    //配置基本工作模式的参数;PWR_UP,EN_CRC,16BIT_CRC,发射模式,开启所有中断

	SPI_NRF_WriteReg(NRF_WRITE_REG+RX_PW_P0,RX_PLOAD_WIDTH);//选择通道0的有效数据宽度
	
    /* Standby-I after power-up settling; Start supplies the CE pulse. */
    if(nrf_io_error == 0U) { Delay_us(1600U); nrf_tx_enabled = 1U; }
}

void NRF_SetRFConfig(uint8_t channel, uint8_t rf_setup)
{

    nrf_config_generation++;
	if(channel > 125U)
	{
		channel = 125U;
	}

	nrf_channel = channel;
	nrf_rf_setup = rf_setup;
}

void NRF_PowerDown(void)
{
    SPI_NRF_WriteReg(NRF_WRITE_REG + CONFIG, 0x0cU);

    nrf_config_generation++;
    NRF_TxCancel();
    nrf_tx_enabled = 0U;
    SPI_NRF_WriteReg(NRF_WRITE_REG + RF_CH, nrf_channel);
    SPI_NRF_WriteReg(NRF_WRITE_REG + RF_SETUP, nrf_rf_setup);
    /* Preserve the CRC contract even if disabled immediately after reset. */
    SPI_NRF_WriteReg(NRF_WRITE_REG + CONFIG, 0x0cU);
}

/**
  * @brief  主要用于NRF与MCU是否正常连接
  * @param  无
  * @retval SUCCESS/ERROR 连接正常/连接失败
  */
u8 NRF_Check(void)
{
	u8 saved_channel, readback;

	/* RF_CH has seven writable bits and is safer to probe than overwriting the
	 * active multi-byte TX address. Two complementary values exercise MOSI/MISO. */
	saved_channel = SPI_NRF_ReadReg(RF_CH);
	if(saved_channel == 0xffU) return ERROR;
	SPI_NRF_WriteReg(NRF_WRITE_REG + RF_CH, 0x2aU);
	readback = SPI_NRF_ReadReg(RF_CH);
	if(readback != 0x2aU) {
		SPI_NRF_WriteReg(NRF_WRITE_REG + RF_CH, saved_channel & 0x7fU);
		return ERROR;
	}
	SPI_NRF_WriteReg(NRF_WRITE_REG + RF_CH, 0x55U);
	readback = SPI_NRF_ReadReg(RF_CH);
	SPI_NRF_WriteReg(NRF_WRITE_REG + RF_CH, saved_channel & 0x7fU);
	if(readback != 0x55U || SPI_NRF_ReadReg(RF_CH) != (saved_channel & 0x7fU))
		return ERROR;
	return nrf_io_error == 0U ? SUCCESS : ERROR;
}

/**
  * @brief   用于向NRF的发送缓冲区中写入数据
  * @param   
  *		@arg txBuf：存储了将要发送的数据的数组，外部定义	
  * @retval  发送结果，成功返回TXDS,失败返回MAXRT或ERROR
  */
void NRF_TxCancel(void)
{
    NRF_CE_LOW();
    nrf_tx_active = 0U;
    SPI_NRF_WriteReg(NRF_WRITE_REG + STATUS, TX_DS | MAX_RT);
    SPI_NRF_WriteReg(FLUSH_TX, NOP);
}

uint8_t NRF_TxStart(uint8_t *txbuf)
{
    uint8_t status;
    if(txbuf == 0 || nrf_io_error != 0U || nrf_tx_enabled == 0U || nrf_tx_active)
        return 0U;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    nrf_tx_started = DWT->CYCCNT;
    NRF_TxCancel(); /* Never send an old MAX_RT payload after a newer command. */
    status = SPI_NRF_ReadReg(STATUS);
    if(nrf_io_error != 0U || status == 0xffU) {
        NRF_IoFault(3U);
        return 0U;
    }
    SPI_NRF_WriteBuf(WR_TX_PLOAD, txbuf, TX_PLOAD_WIDTH);
    if(nrf_io_error != 0U) return 0U;
    nrf_tx_active = 1U;
    NRF_CE_HIGH();
    Delay_us(20U);
    NRF_CE_LOW();
    return 1U;
}

uint8_t NRF_TxPoll(void)
{
    uint8_t status, result;
    if(nrf_tx_active == 0U || nrf_io_error != 0U) return ERROR;
    if((uint32_t)(DWT->CYCCNT - nrf_tx_started) >=
       (SystemCoreClock / 1000U) * NRF_TX_TIMEOUT_MS) {
        NRF_TxCancel();
        return ERROR;
    }
    status = SPI_NRF_ReadReg(STATUS);
    if(nrf_io_error != 0U || status == 0xffU) {
        NRF_IoFault(3U);
        return ERROR;
    }
    if((status & (TX_DS | MAX_RT)) == 0U) return NRF_TX_PENDING;
    result = (status & MAX_RT) ? MAX_RT : TX_DS;
    NRF_TxCancel();
    return nrf_io_error == 0U ? result : ERROR;
}

u8 NRF_Tx_Dat(u8 *txbuf)
{
    uint16_t polls;
    uint8_t status;
    if(!NRF_TxStart(txbuf)) return ERROR;
    for(polls = 0U; polls < 300U; polls++) {
        status = NRF_TxPoll();
        if(status != NRF_TX_PENDING) return status;
        Delay_us(100U);
    }
    NRF_TxCancel();
    return ERROR;
} 

/**
  * @brief   用于从NRF的接收缓冲区中读出数据
  * @param   
  *		@arg rxBuf ：用于接收该数据的数组，外部定义	
  * @retval 
  *		@arg 接收结果
  */
u8 NRF_Rx_Dat(u8 *rxbuf)
{
    uint8_t state;
    if(rxbuf == 0 || nrf_io_error != 0U) return ERROR;
    NRF_CE_HIGH();
    if(NRF_Read_IRQ() != 0U) return ERROR;
    state = SPI_NRF_ReadReg(STATUS);
    if(nrf_io_error != 0U || state == 0xffU) {
        NRF_IoFault(3U);
        return ERROR;
    }
    if((state & RX_DR) == 0U) return ERROR;
    SPI_NRF_ReadBuf(RD_RX_PLOAD, rxbuf, RX_PLOAD_WIDTH);
    SPI_NRF_WriteReg(NRF_WRITE_REG + STATUS, RX_DR);
    SPI_NRF_WriteReg(FLUSH_RX, NOP);
    if(nrf_io_error != 0U) return ERROR;
    NRF_CE_HIGH();
    return RX_DR;
}


/*********************************************END OF FILE**********************/

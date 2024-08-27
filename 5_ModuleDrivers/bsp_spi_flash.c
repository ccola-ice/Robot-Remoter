#include "bsp_spi_flash.h"

volatile uint32_t flash_count_wait = FLASH_TIME_OUT;

 /**
  * @brief  W25Q128 SPI1 GPIO初始化
  * @param  无
  * @retval 无
  */
static void FLASH_SPI_GPIO_Config(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    
    /* 使能SPI1对应GPIO引脚时钟 */
    FLASH_SPI_MISO_GPIO_CLK_INIT( FLASH_SPI_MISO_GPIO_CLK, ENABLE);
    FLASH_SPI_MOSI_GPIO_CLK_INIT( FLASH_SPI_MOSI_GPIO_CLK, ENABLE);
    FLASH_SPI_SCK_GPIO_CLK_INIT(  FLASH_SPI_SCK_GPIO_CLK,  ENABLE);
    FLASH_SPI_CS_GPIO_CLK_INIT(   FLASH_SPI_CS_GPIO_CLK,   ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    
    /* 连接 PXx引脚 到 SPI1 外设 F4XX系列特有*/
    GPIO_PinAFConfig(FLASH_SPI_MISO_GPIO_PORT, FLASH_SPI_MISO_SOURCE, FLASH_SPI_MISO_AF);
    GPIO_PinAFConfig(FLASH_SPI_MOSI_GPIO_PORT, FLASH_SPI_MOSI_SOURCE, FLASH_SPI_MOSI_AF);
    GPIO_PinAFConfig(FLASH_SPI_SCK_GPIO_PORT,  FLASH_SPI_SCK_SOURCE,  FLASH_SPI_SCK_AF);

    /* SPI1 SCK GPIO初始化 */
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Pin = FLASH_SPI_SCK_GPIO_PIN  ;
    GPIO_Init(FLASH_SPI_SCK_GPIO_PORT, &GPIO_InitStructure);
    /* SPI1 MOSI GPIO初始化 */
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Pin = FLASH_SPI_MOSI_GPIO_PIN  ;
    GPIO_Init(FLASH_SPI_MOSI_GPIO_PORT, &GPIO_InitStructure);
    /* SPI1 MISO GPIO初始化 F4XX系列的MISO GPIO也直接配置为推挽输出，但也可以读取IO状态*/
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Pin = FLASH_SPI_MISO_GPIO_PIN  ;
    GPIO_Init(FLASH_SPI_MISO_GPIO_PORT, &GPIO_InitStructure);
    /* SPI1 CS GPIO初始化 软件控制CS引脚*/
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_Pin = FLASH_SPI_CS_GPIO_PIN  ;
    GPIO_Init(FLASH_SPI_CS_GPIO_PORT, &GPIO_InitStructure);
}


 /**
  * @brief  W25Q128 SPI1 工作模式初始化
  * @param  无
  * @retval 无
  */
static void FLASH_SPI_MODE_Config(void)
{
    SPI_InitTypeDef   SPI_InitStructure;
    
    /* 使能SPI1时钟 */
    FLASH_SPI_CLK_INIT(FLASH_SPI_CLK, ENABLE);
    
    /* SPI1 初始化 mode 0*/
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2 ;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge ;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;
    SPI_InitStructure.SPI_CRCPolynomial = 7;//随便写的
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_Init(FLASH_SPI,&SPI_InitStructure);

    /* 使能 SPI1 */
    SPI_Cmd(FLASH_SPI,ENABLE);
}


//W25Q128 SPI1初始化
void FLASH_SPI_Init(void)
{
    FLASH_SPI_GPIO_Config();
	FLASH_SPI_CS_HIGH;
    FLASH_SPI_MODE_Config();
}


 /**
  * @brief  使用SPI发送一个字节的数据
  * @param  byte：要发送的数据
  * @retval 返回接收到的数据
  */
static uint8_t FLASH_Send_Byte(uint8_t byte)
{
    uint8_t return_data;
    
    flash_count_wait =  FLASH_TIME_OUT;
    /* 等待发送缓冲区为空，TXE事件 */
    while(SPI_I2S_GetFlagStatus(FLASH_SPI, SPI_I2S_FLAG_TXE) == RESET)
    {
        flash_count_wait --;
        if(flash_count_wait == 0)
        {
            return FLASH_Error_CallBack(1);
        }
    }
    /* 写入数据寄存器，把要写入的数据写入发送缓冲区 */
    SPI_I2S_SendData(FLASH_SPI,byte);
    
    flash_count_wait =  FLASH_TIME_OUT;
    /* 等待接收缓冲区非空，RXNE事件 */
    while(SPI_I2S_GetFlagStatus(FLASH_SPI, SPI_I2S_FLAG_RXNE) == RESET)
    {
        flash_count_wait --;
        if(flash_count_wait == 0)
        {
            return FLASH_Error_CallBack(2);
        }
    }  
    /* 读取数据寄存器，获取接收缓冲区数据 */
    return_data = SPI_I2S_ReceiveData(FLASH_SPI);
    
    return return_data;
}


 /**
  * @brief  使用SPI读取一个字节的数据
  * @param  无
  * @retval 返回接收到的数据
  */
static uint8_t FLASH_Receive_Byte(void)
{
  return (FLASH_Send_Byte(0xFF));//Dummy_Byte
}

 /**
  * @brief  读取Flash Device ID [ID7-ID0]
  * @param  无
  * @retval 返回Device ID
  */
uint8_t FLASH_Read_DeviceID(void)
{
    uint8_t DeviceID_Value;
    
    //拉低CS信号，开始通信
    FLASH_SPI_CS_LOW;
    
    //写指令代码W25X_ReleasePowerDown，和3个Dummy Bytes
    FLASH_Send_Byte(W25X_ReleasePowerDown);
    FLASH_Send_Byte(0xFF);
    FLASH_Send_Byte(0xFF);
    FLASH_Send_Byte(0xFF);
    
    //接收读取到的内容
    DeviceID_Value = FLASH_Receive_Byte();
    
    //拉高CS信号，停止通信
    FLASH_SPI_CS_HIGH;
    
    return DeviceID_Value;
}

 /**
  * @brief  读取FLASH ID
  * @param 	无
  * @retval FLASH ID
  */
uint32_t FLASH_Read_FlashID(void)
{
  uint32_t Temp = 0, Temp0 = 0, Temp1 = 0, Temp2 = 0;

  //拉低CS信号，开始通信
  FLASH_SPI_CS_LOW;

  //写指令代码W25X_JedecDeviceID，和3个Dummy Bytes
  FLASH_Send_Byte(W25X_JedecDeviceID);

  Temp0 = FLASH_Send_Byte(0xFF);
  Temp1 = FLASH_Send_Byte(0xFF);
  Temp2 = FLASH_Send_Byte(0xFF);

  //拉高CS信号，停止通信
  FLASH_SPI_CS_HIGH;

  //把数据组合起来
  Temp = (Temp0 << 16) | (Temp1 << 8) | Temp2;

  return Temp;
}

 /**
  * @brief  擦除扇区，每次最少擦除4096字节（所有的存储单元都被写1）
  * @param  SectorAddr,必须对齐到要擦除的扇区的首地址 0,4096,......
  * @retval 无
  */
void FLASH_Erase_Sectors(uint32_t SectorAddr)
{
    uint8_t addr_low,addr_mid,addr_high;
    
    addr_low  = (SectorAddr >> 0)  & 0x000000FF;
    addr_mid  = (SectorAddr >> 8)  & 0x000000FF;
    addr_high = (SectorAddr >> 16) & 0x000000FF;
    
    /* 写使能 */
    Flash_Write_Enable();
	Flash_Wait_For_Standby();
    
    /* 擦除扇区 */
    //拉低CS信号，开始通信
    FLASH_SPI_CS_LOW;
    
    //发送擦除扇区指令代码W25X_SectorErase
    FLASH_Send_Byte(W25X_SectorErase);
    
    //发送要擦除的地址
    FLASH_Send_Byte(addr_high);
    FLASH_Send_Byte(addr_mid);
    FLASH_Send_Byte(addr_low);

    //拉高CS信号，停止通信
    FLASH_SPI_CS_HIGH;
    
    //等待内部时序完成 ，W25Q128要求在写入/擦除/其他动作前，需要看W25Q128是否为空闲状态，只有空闲状态才能进行操作。
    Flash_Wait_For_Standby();    
}

 /**
  * @brief  擦除FLASH扇区，整片擦除
  * @param  无
  * @retval 无
  */
void FLASH_Erase_Bulk(void)
{
	/* 写使能 */
	Flash_Write_Enable();

	/* 整块 Erase */
	//拉低CS信号，开始通信
	FLASH_SPI_CS_LOW;
	
	/* 发送整块擦除指令*/
	FLASH_Send_Byte(W25X_ChipErase);
	
	//拉高CS信号，停止通信
	FLASH_SPI_CS_HIGH;

	//等待擦除完毕
	Flash_Wait_For_Standby();
}


 /**
  * @brief  读取FLASH数据
  * @param  data：存储要读取的数据的指针 
            ReadAddr：要读取的数据的地址
            size：要读取的数据长度，以字节为单位（最多能读0xFFFFFF=16,777,215个字节）
  * @retval 无
  */
void FLASH_Read_Data(uint8_t * data,uint32_t ReadAddr,uint16_t size)
{
    uint8_t addr_low,addr_mid,addr_high;
    
    addr_low  = (ReadAddr >> 0)  & 0x000000FF;
    addr_mid  = (ReadAddr >> 8)  & 0x000000FF;
    addr_high = (ReadAddr >> 16) & 0x000000FF;
    
    //拉低CS信号，开始通信
    FLASH_SPI_CS_LOW;
    
    //发送指令代码W25X_ReadData
    FLASH_Send_Byte(W25X_ReadData);
    
    //发送要读取数据的地址
    FLASH_Send_Byte(addr_high);
    FLASH_Send_Byte(addr_mid);
    FLASH_Send_Byte(addr_low);

    while(size--)
    {
        *data = FLASH_Receive_Byte();
        data++;
    }
    
    //拉高CS信号，停止通信
    FLASH_SPI_CS_HIGH;     
}


 /**
  * @brief 写入数据v1.0   数据长度不超过256 调用本函数写入数据前需要先擦除扇区
  * @param  addr：要写入的数据的Flash地址 
            data：要写入的数据 的指针
            size：要写入的数据长度，以字节为单位 不超过256
  * @retval
  */
void FLASH_Write_Page_v1(uint32_t addr,uint8_t * data,uint16_t size)
{
    uint8_t addr_low,addr_mid,addr_high;
    
    addr_low  = (addr >> 0)  & 0x000000FF;
    addr_mid  = (addr >> 8)  & 0x000000FF;
    addr_high = (addr >> 16) & 0x000000FF;
    
    /* 写使能 */
    Flash_Write_Enable();
    
    //拉低CS信号，开始通信
    FLASH_SPI_CS_LOW;
    
    //发送指令代码W25X_PageProgram
    FLASH_Send_Byte(W25X_PageProgram);
    
    //发送要写入Flash的地址
    FLASH_Send_Byte(addr_high);
    FLASH_Send_Byte(addr_mid);
    FLASH_Send_Byte(addr_low);

    //发送要写入的数据 
    while(size--)
    {
       FLASH_Send_Byte(*data);
       data++;
    }
    
    //拉高CS信号，停止通信
    FLASH_SPI_CS_HIGH;
    
    //等待内部时序完成 ，W25Q128要求在写入/擦除/其他动作前，需要看W25Q128是否为空闲状态，只有空闲状态才能进行操作。
    Flash_Wait_For_Standby();   
}


 /**
  * @brief 写入数据v2.0  数据长度最大4096 效率低，占用资源高 调用本函数写入数据前需要先擦除扇区
  * @param  addr：要写入的数据的Flash地址 
            data：要写入的数据 的指针
            size：要写入的数据长度，长度不受限制，最大4096
  * @retval
  */
void FLASH_Write_Page_v2(uint32_t addr,uint8_t * data,uint16_t size)
{
    uint8_t addr_low,addr_mid,addr_high;
    
    while(size--)
    {
        addr_low  = (addr >> 0)  & 0x000000FF;
        addr_mid  = (addr >> 8)  & 0x000000FF;
        addr_high = (addr >> 16) & 0x000000FF;
        
        /* 写使能 */
        Flash_Write_Enable();
        
        //拉低CS信号，开始通信
        FLASH_SPI_CS_LOW;
        
        //发送指令代码W25X_PageProgram
        FLASH_Send_Byte(W25X_PageProgram);
        
        //发送要写入Flash的地址
        FLASH_Send_Byte(addr_high);
        FLASH_Send_Byte(addr_mid);
        FLASH_Send_Byte(addr_low);

        //发送要写入的数据 
        FLASH_Send_Byte(*data);
        data++;
        
        //因为现在的逻辑是每发送一个字节的数据都先发送该字节的存储单元地址，所以存储单元地址要自增
        addr++;
        
        //拉高CS信号，停止通信
        FLASH_SPI_CS_HIGH;
        
        //等待内部时序完成 ，W25Q128要求在写入/擦除/其他动作前，需要看W25Q128是否为空闲状态，只有空闲状态才能进行操作。
        Flash_Wait_For_Standby();
    }      
}


 /**
  * @brief 写入数据v3.0 按页写入 数据长度最大4096 效率更高，占用资源低 且地址任意,可以跨扇区写入 调用本函数写入数据前需要先擦除扇区
  * @param  data：要写入的数据 的指针
            WriteAddr：要写入的数据的Flash地址 
            size：要写入的数据长度，长度不受限制，最大4096
  * @retval
  */
void FLASH_Write_Page_v3(uint8_t * data,uint32_t WriteAddr,uint16_t size)
{
    uint8_t addr_low,addr_mid,addr_high;
    uint32_t count = 0 ;
	
    Flash_Write_Enable();
    
	while(size--)
    {
        addr_low  = (WriteAddr >> 0)  & 0x000000FF;
        addr_mid  = (WriteAddr >> 8)  & 0x000000FF;
        addr_high = (WriteAddr >> 16) & 0x000000FF;
        
        count ++;
        
		if(size > FLASH_PerWritePageSize)
		{
			size = FLASH_PerWritePageSize;
			FLASH_ERROR("SPI_FLASH_PageWrite too large!");
		}	
		
        //分别在第1,256*1+1,256*2+1,256*3+1...以及addr地址4096对齐时进入if 进行一次W25X_PageProgram操作
        //其中，除了第一次之外， 后面每次进入if都要先等待内部时序完成，因为Flash规定每写入/擦除256个字节就要等待写入完成，再发送下一次256个字节的命令和首地址。
        if(count == 1 || (count%256) == 1 || (WriteAddr%4096) == 0)
        {            
            /*结束上一次的页写入指令(256个字节)*/
            //拉高CS信号，停止通信
            FLASH_SPI_CS_HIGH;
            
            /*这里还要加入一次等待写入时序完成，等待上一次256个数据写入的内部时序完成*/
            //等待内部时序完成 ，W25Q128要求在写入/擦除/其他动作前，需要看W25Q128是否为空闲状态，只有空闲状态才能进行操作。
            Flash_Wait_For_Standby();
            
            /* 写使能 */
            Flash_Write_Enable();
        
            //拉低CS信号，开始通信
            FLASH_SPI_CS_LOW;
        
            //发送指令代码W25X_PageProgram
            FLASH_Send_Byte(W25X_PageProgram);
            
            //发送要写入Flash的地址
            FLASH_Send_Byte(addr_high);
            FLASH_Send_Byte(addr_mid);
            FLASH_Send_Byte(addr_low);   
        }
        
        //发送要写入的数据 
        FLASH_Send_Byte(*data);
        data++;
        //因为现在的逻辑是每发送一个字节的数据都先发送该字节的存储单元地址，所以存储单元地址要自增
        WriteAddr++;
    
    }
    //拉高CS信号，停止通信
    FLASH_SPI_CS_HIGH;
    //等待内部时序完成 ，W25Q128要求在写入/擦除/其他动作前，需要看W25Q128是否为空闲状态，只有空闲状态才能进行操作。
    Flash_Wait_For_Standby();     
}

 /**
  * @brief  对FLASH写入数据，调用本函数写入数据前需要先擦除扇区
  * @param	data，要写入数据的 指针
  * @param  WriteAddr，写入地址
  * @param  size，写入数据长度
  * @retval 无
  */
void FLASH_Write_Data(uint8_t * data, uint32_t WriteAddr, uint16_t size)
{
	  uint8_t NumOfPage = 0, NumOfSingle = 0, Addr = 0, count = 0, temp = 0;
		
		/*mod运算求余，若writeAddr是FLASH_PageSize整数倍，运算结果Addr值为0*/
	  Addr = WriteAddr % FLASH_PageSize;
		
		/*差count个数据值，刚好可以对齐到页地址*/
	  count = FLASH_PageSize - Addr;	
		/*计算出要写多少整数页*/
	  NumOfPage =  size / FLASH_PageSize;
		/*mod运算求余，计算出剩余不满一页的字节数*/
	  NumOfSingle = size % FLASH_PageSize;

		 /* Addr=0,则WriteAddr 刚好按页对齐 aligned  */
	  if (Addr == 0) 
	  {
			/* size < FLASH_PageSize */
		if (NumOfPage == 0) 
		{
		  FLASH_Write_Page_v3(data, WriteAddr, size);
		}
		else /* size > FLASH_PageSize */
		{
				/*先把整数页都写了*/
		  while (NumOfPage--)
		  {
			FLASH_Write_Page_v3(data, WriteAddr, FLASH_PageSize);
			WriteAddr +=  FLASH_PageSize;
			data += FLASH_PageSize;
		  }
				
				/*若有多余的不满一页的数据，把它写完*/
		  FLASH_Write_Page_v3(data, WriteAddr, NumOfSingle);
		}
	  }
		/* 若地址与 FLASH_PageSize 不对齐  */
	  else 
	  {
			/* size < FLASH_PageSize */
		if (NumOfPage == 0) 
		{
				/*当前页剩余的count个位置比NumOfSingle小，写不完*/
		  if (NumOfSingle > count) 
		  {
			temp = NumOfSingle - count;
					
					/*先写满当前页*/
			FLASH_Write_Page_v3(data, WriteAddr, count);
			WriteAddr +=  count;
			data += count;
					
					/*再写剩余的数据*/
			FLASH_Write_Page_v3(data, WriteAddr, temp);
		  }
		  else /*当前页剩余的count个位置能写完NumOfSingle个数据*/
		  {				
			FLASH_Write_Page_v3(data, WriteAddr, size);
		  }
		}
		else /* size > FLASH_PageSize */
		{
				/*地址不对齐多出的count分开处理，不加入这个运算*/
		  size -= count;
		  NumOfPage =  size / FLASH_PageSize;
		  NumOfSingle = size % FLASH_PageSize;

		  FLASH_Write_Page_v3(data, WriteAddr, count);
		  WriteAddr +=  count;
		  data += count;
				
				/*把整数页都写了*/
		  while (NumOfPage--)
		  {
			FLASH_Write_Page_v3(data, WriteAddr, FLASH_PageSize);
			WriteAddr +=  FLASH_PageSize;
			data += FLASH_PageSize;
		  }
				/*若有多余的不满一页的数据，把它写完*/
		  if (NumOfSingle != 0)
		  {
			FLASH_Write_Page_v3(data, WriteAddr, NumOfSingle);
		  }
		}
	  }
}

//进入掉电模式
void FLASH_PowerDown(void)   
{ 
  /* 选择 FLASH: CS 低 */
  FLASH_SPI_CS_LOW;

  /* 发送 掉电 命令 */
  FLASH_Send_Byte(W25X_PowerDown);

  /* 停止信号  FLASH: CS 高 */
  FLASH_SPI_CS_HIGH;
} 

//唤醒
void FLASH_Wakeup(void)   
{
  /*选择 FLASH: CS 低 */
  FLASH_SPI_CS_LOW;

  /* 发上 上电 命令 */
  FLASH_Send_Byte(W25X_ReleasePowerDown);

  /* 停止信号 FLASH: CS 高 */
  FLASH_SPI_CS_HIGH;                   //等待TRES1
}


 /**
  * @brief  写使能
  * @param  无
  * @retval 无
  */
static void Flash_Write_Enable(void)
{
    //拉低CS信号，开始通信
    FLASH_SPI_CS_LOW;
    
    //指令代码W25X_WriteEnable
    FLASH_Send_Byte(W25X_WriteEnable);

    //拉高CS信号，停止通信
    FLASH_SPI_CS_HIGH;
}


 /**
  * @brief  一直等待，知道Flash变为空闲状态 
  * @param  无
  * @retval 无
  */
static void Flash_Wait_For_Standby(void)
{
    uint8_t Flash_status = 0;
    
    //拉低CS信号，开始通信
    FLASH_SPI_CS_LOW;
    
    //发送指令代码W25X_ReadStatusReg
    FLASH_Send_Byte(W25X_ReadStatusReg);
    
    //接收返回数据，返回的就是状态寄存器的值
    Flash_status = FLASH_Receive_Byte();
    
    flash_count_wait =  FLASH_TIME_OUT;
    /*检测FLASH的状态寄存器的最低位(BUSY位)，如果BUSY位为1则处于忙碌状态，否则为空闲状态*/
    while(1)
    {
        Flash_status = FLASH_Receive_Byte();
        if((Flash_status & WIP_Flag) == 0)
        {
            break;
        }
        flash_count_wait -- ;
        if(flash_count_wait == 0)
        {
            FLASH_Error_CallBack(3);
            break;
        }
    }                              
    
    //拉高CS信号，停止通信
    FLASH_SPI_CS_HIGH;
}


//code：错误编码
static uint8_t FLASH_Error_CallBack(uint8_t code)
{
    printf("\r\nSPI error occurred , code = %d",code);
    return code;
}

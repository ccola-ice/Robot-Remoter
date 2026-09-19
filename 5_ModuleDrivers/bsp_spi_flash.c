#include "bsp_spi_flash.h"
#include "spi_flash_layout.h"
#include "bsp_SysTick.h"
#include <string.h>

static uint8_t flash_io_error;

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
    if(flash_io_error != 0U) return 0xffU;
    
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
void FLASH_Erase_Sectors(uint32_t address)
{
    if(flash_io_error != 0U) return;
    if(address >= SPI_FLASH_LAYOUT_TOTAL_SIZE ||
       address % SPI_FLASH_LAYOUT_SECTOR_SIZE != 0UL) {
        FLASH_Error_CallBack(4U);
        return;
    }
    if(Flash_Wait_For_Standby(3000U) != 0U) return;
    Flash_Write_Enable();
    if(flash_io_error != 0U) return;
    FLASH_SPI_CS_LOW;
    FLASH_Send_Byte(W25X_SectorErase);
    FLASH_Send_Byte((uint8_t)(address >> 16));
    FLASH_Send_Byte((uint8_t)(address >> 8));
    FLASH_Send_Byte((uint8_t)address);
    FLASH_SPI_CS_HIGH;
    (void)Flash_Wait_For_Standby(3000U);
}

 /**
  * @brief  擦除FLASH扇区，整片擦除
  * @param  无
  * @retval 无
  */
void FLASH_Erase_Bulk(void)
{
	/* 写使能 */
	if(Flash_Wait_For_Standby(3000U) != 0U) return;
	Flash_Write_Enable();
	if(flash_io_error != 0U) return;

	/* 整块 Erase */
	//拉低CS信号，开始通信
	FLASH_SPI_CS_LOW;
	
	/* 发送整块擦除指令*/
	FLASH_Send_Byte(W25X_ChipErase);
	
	//拉高CS信号，停止通信
	FLASH_SPI_CS_HIGH;

	//等待擦除完毕
	if(Flash_Wait_For_Standby(300000U) != 0U) return;
}


 /**
  * @brief  读取FLASH数据
  * @param  data：存储要读取的数据的指针 
            ReadAddr：要读取的数据的地址
            size：要读取的数据长度，以字节为单位（最多能读0xFFFFFF=16,777,215个字节）
  * @retval 无
  */
void FLASH_Read_Data(uint8_t *data, uint32_t address, uint16_t size)
{
    if(flash_io_error != 0U || size == 0U) return;
    if(data == 0 || address >= SPI_FLASH_LAYOUT_TOTAL_SIZE ||
       (uint32_t)size > SPI_FLASH_LAYOUT_TOTAL_SIZE - address) {
        FLASH_Error_CallBack(4U);
        return;
    }
    if(Flash_Wait_For_Standby(3000U) != 0U) return;
    FLASH_SPI_CS_LOW;
    FLASH_Send_Byte(W25X_ReadData);
    FLASH_Send_Byte((uint8_t)(address >> 16));
    FLASH_Send_Byte((uint8_t)(address >> 8));
    FLASH_Send_Byte((uint8_t)address);
    while(size-- != 0U && flash_io_error == 0U) *data++ = FLASH_Receive_Byte();
    FLASH_SPI_CS_HIGH;
}


 /**
  * @brief 写入数据v1.0   数据长度不超过256 调用本函数写入数据前需要先擦除扇区
  * @param  addr：要写入的数据的Flash地址 
            data：要写入的数据 的指针
            size：要写入的数据长度，以字节为单位 不超过256
  * @retval
  */
void FLASH_Write_Page_v1(uint32_t address, uint8_t *data, uint16_t size)
{
    if(flash_io_error != 0U || size == 0U) return;
    if(data == 0 || address >= SPI_FLASH_LAYOUT_TOTAL_SIZE ||
       (uint32_t)size > SPI_FLASH_LAYOUT_TOTAL_SIZE - address ||
       size > FLASH_PageSize - (address % FLASH_PageSize)) {
        FLASH_Error_CallBack(4U);
        return;
    }
    if(Flash_Wait_For_Standby(3000U) != 0U) return;
    Flash_Write_Enable();
    if(flash_io_error != 0U) return;
    FLASH_SPI_CS_LOW;
    FLASH_Send_Byte(W25X_PageProgram);
    FLASH_Send_Byte((uint8_t)(address >> 16));
    FLASH_Send_Byte((uint8_t)(address >> 8));
    FLASH_Send_Byte((uint8_t)address);
    while(size-- != 0U && flash_io_error == 0U) FLASH_Send_Byte(*data++);
    FLASH_SPI_CS_HIGH;
    (void)Flash_Wait_For_Standby(3000U);
}


 /**
  * @brief 写入数据v2.0  数据长度最大4096 效率低，占用资源高 调用本函数写入数据前需要先擦除扇区
  * @param  addr：要写入的数据的Flash地址 
            data：要写入的数据 的指针
            size：要写入的数据长度，长度不受限制，最大4096
  * @retval
  */
void FLASH_Write_Page_v2(uint32_t address, uint8_t *data, uint16_t size)
{
    FLASH_Write_Data(data, address, size);
}


 /**
  * @brief 写入数据v3.0 按页写入 数据长度最大4096 效率更高，占用资源低 且地址任意,可以跨扇区写入 调用本函数写入数据前需要先擦除扇区
  * @param  data：要写入的数据 的指针
            WriteAddr：要写入的数据的Flash地址 
            size：要写入的数据长度，长度不受限制，最大4096
  * @retval
  */
void FLASH_Write_Page_v3(uint8_t *data, uint32_t address, uint16_t size)
{
    FLASH_Write_Data(data, address, size);
}

 /**
  * @brief  对FLASH写入数据，调用本函数写入数据前需要先擦除扇区
  * @param	data，要写入数据的 指针
  * @param  WriteAddr，写入地址
  * @param  size，写入数据长度
  * @retval 无
  */
void FLASH_Write_Data(uint8_t *data, uint32_t address, uint16_t size)
{
    uint16_t chunk;
    if(flash_io_error != 0U || size == 0U) return;
    if(data == 0 || address >= SPI_FLASH_LAYOUT_TOTAL_SIZE ||
       (uint32_t)size > SPI_FLASH_LAYOUT_TOTAL_SIZE - address) {
        FLASH_Error_CallBack(4U);
        return;
    }
    while(size != 0U) {
        /* W25Q128 Page Program wraps within 256 bytes, regardless of the
         * 4 KiB erase-sector size. Chunk by physical address, not count. */
        chunk = (uint16_t)(FLASH_PageSize - (address % FLASH_PageSize));
        if(chunk > size) chunk = size;
        FLASH_Write_Page_v1(address, data, chunk);
        if(flash_io_error != 0U) return;
        address += chunk;
        data += chunk;
        size = (uint16_t)(size - chunk);
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
    uint8_t status;
    if(flash_io_error != 0U) return;
    FLASH_SPI_CS_LOW;
    FLASH_Send_Byte(W25X_WriteEnable);
    FLASH_SPI_CS_HIGH;
    FLASH_SPI_CS_LOW;
    FLASH_Send_Byte(W25X_ReadStatusReg);
    status = FLASH_Receive_Byte();
    FLASH_SPI_CS_HIGH;
    if(flash_io_error == 0U && (status & 0x02U) == 0U) FLASH_Error_CallBack(5U);
}


 /**
  * @brief  一直等待，知道Flash变为空闲状态 
  * @param  无
  * @retval 无
  */
static uint8_t Flash_Wait_For_Standby(uint32_t timeout_ms)
{
    uint8_t status;
    while(timeout_ms-- != 0U) {
        FLASH_SPI_CS_LOW;
        FLASH_Send_Byte(W25X_ReadStatusReg);
        status = FLASH_Receive_Byte();
        FLASH_SPI_CS_HIGH;
        if(flash_io_error != 0U) return 1U;
        if((status & WIP_Flag) == 0U) return 0U;
        Delay_ms(1U);
    }
    FLASH_Error_CallBack(3U);
    return 1U;
}

//code：错误编码
static uint8_t FLASH_Error_CallBack(uint8_t code)
{
    flash_io_error = code;
    printf("\r\nSPI error occurred , code = %d",code);
    return code;
}

/* Read-only boot probe. No WREN, erase, program or status-register write. */
uint8_t FLASH_BootProbe(uint32_t *jedec_id)
{
    static const uint32_t addresses[] = {
        0UL, SPI_FLASH_PARAM_ADDR, SPI_FLASH_LAYOUT_TOTAL_SIZE - 32UL
    };
    uint8_t first[32], second[32];
    uint8_t i, status = 0xffU;
    uint16_t tries;
    uint32_t id, repeat;

    flash_io_error = 0U;
    if(jedec_id != 0) *jedec_id = 0UL;
    FLASH_Wakeup();
    Delay_us(30U);

    /* A warm MCU reset may occur while the flash is finishing a prior write. */
    for(tries = 0U; tries < 500U; tries++) {
        FLASH_SPI_CS_LOW;
        FLASH_Send_Byte(W25X_ReadStatusReg);
        status = FLASH_Receive_Byte();
        FLASH_SPI_CS_HIGH;
        if(flash_io_error != 0U) return 1U;
        if((status & WIP_Flag) == 0U) break;
        Delay_ms(1U);
    }
    if((status & WIP_Flag) != 0U) return 3U;
    id = FLASH_Read_FlashID();
    repeat = FLASH_Read_FlashID();
    if(jedec_id != 0) *jedec_id = id;
    if(flash_io_error != 0U) return 1U;
    if(id != FLASH_ID || repeat != id) return 2U;
    for(i = 0U; i < sizeof(addresses) / sizeof(addresses[0]); i++) {
        FLASH_Read_Data(first, addresses[i], sizeof(first));
        FLASH_Read_Data(second, addresses[i], sizeof(second));
        if(flash_io_error != 0U) return 1U;
        if(memcmp(first, second, sizeof(first)) != 0) return 4U;
    }
    return 0U; /* An erased (all-FF) data area is valid, not a hardware fault. */
}

uint8_t FLASH_GetIoError(void)
{
    return flash_io_error;
}

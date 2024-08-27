#include "bsp_i2c_eeprom.h"

uint32_t eeprom_count_wait = EEPROM_TIME_OUT;

 /**
  * @brief  EEPROM I2C1 GPIO初始化
  * @param  无
  * @retval 无
  */
static void EEPROM_I2C_GPIO_Config(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    
    /* 使能I2C1对应GPIO引脚时钟 */
    EEPROM_I2C_SDA_GPIO_CLK_INIT( EEPROM_I2C_SDA_GPIO_CLK, ENABLE);
    EEPROM_I2C_SCL_GPIO_CLK_INIT( EEPROM_I2C_SCL_GPIO_CLK, ENABLE);

    /* 连接 PXx引脚 到 I2C SCL外设 F4XX系列特有(即默认情况下外设没有连到对应引脚，要通过此将外设连接到对应引脚) */
    GPIO_PinAFConfig(EEPROM_I2C_SDA_GPIO_PORT,EEPROM_I2C_SDA_SOURCE,EEPROM_I2C_SDA_AF);
    GPIO_PinAFConfig(EEPROM_I2C_SCL_GPIO_PORT,EEPROM_I2C_SCL_SOURCE,EEPROM_I2C_SCL_AF);

    /* I2C1 SDA GPIO初始化 */
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;      //必须设置为开漏输出
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;        //复用功能
    GPIO_InitStructure.GPIO_Pin = EEPROM_I2C_SDA_GPIO_PIN  ;
    GPIO_Init(EEPROM_I2C_SDA_GPIO_PORT, &GPIO_InitStructure);
    /* I2C1 SCL GPIO初始化 */
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Pin = EEPROM_I2C_SCL_GPIO_PIN  ;
    GPIO_Init(EEPROM_I2C_SCL_GPIO_PORT, &GPIO_InitStructure);
}


 /**
  * @brief  EEPROM I2C1 工作模式初始化
  * @param  无
  * @retval 无
  */
static void EEPROM_I2C_MODE_Config(void)
{
    I2C_InitTypeDef   I2C_InitStructure;
    
    /* 使能I2C1时钟 */
    EEPROM_I2C_CLK_INIT( EEPROM_I2C_CLK, ENABLE);
    
    /* I2C1 初始化 */
    I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_InitStructure.I2C_ClockSpeed = I2C_SPEED;
    I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
    I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStructure.I2C_OwnAddress1 = STM32_I2C_OWN_ADDR;    //STM32的I2C设备地址。
    
    I2C_Init(EEPROM_I2C,&I2C_InitStructure);

    /* 使能 I2C1 */
    I2C_Cmd(EEPROM_I2C,ENABLE);
    
    I2C_AcknowledgeConfig(EEPROM_I2C, ENABLE);
}

//EEPROM I2C初始化
void EEPROM_I2C_Init(void)
{
    EEPROM_I2C_GPIO_Config();
    EEPROM_I2C_MODE_Config();
}


//addr:要写入的存储单元地址
//data:要写入的数据
//note:只能写入一个字节
//return :0表示正常，非0为失败
uint8_t EEPROM_Byte_Write(uint8_t addr ,uint8_t data)
{
    //(有关事件的内容详见野火手册的 24.2.3通讯过程 )
    
    //产生起始信号
    I2C_GenerateSTART(EEPROM_I2C,ENABLE);
    
    //重置eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //等待EV5事件，直到检测成功
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_MODE_SELECT) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //打印错误编码并返回
            Error_CallBack(1);
        }
    }
    
    //向I2C1总线广播EEPROM设备地址，并设置写方向
    I2C_Send7bitAddress(EEPROM_I2C,EEPROM_I2C_ADDR,I2C_Direction_Transmitter);
    
    //重置eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //等待EV6事件，直到检测成功
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //打印错误编码并返回
            Error_CallBack(2);
        }
    }
    
    //发送要写入的EEPROM存储单元格地址
    I2C_SendData(EEPROM_I2C,addr);
    
    //重置eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //等待EV8_2事件，直到检测成功
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_BYTE_TRANSMITTED) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //打印错误编码并返回
            Error_CallBack(3);
        }
    }
    
    //发送要写入的数据
    I2C_SendData(EEPROM_I2C,data);
    
    //重置eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //等待EV8_2事件，直到检测成功
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_BYTE_TRANSMITTED) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //打印错误编码并返回
            Error_CallBack(4);
        }
    }
    
    //产生结束信号
    I2C_GenerateSTOP(EEPROM_I2C,ENABLE);

    //等待EEPROM写入时序完成
    return Wait_For_Standby();
}


//addr:要读取的EEPROM单元格地址
//*data:读取到的数据指针，读取到的数据存储在这个指针所指向的地方(即地址).
//return :0表示正常，非0为失败
uint8_t EEPROM_Random_Read(uint8_t addr , uint8_t *data)
{
    //产生起始信号
    I2C_GenerateSTART(EEPROM_I2C,ENABLE);
    
    //重置eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //等待EV5事件，直到检测成功
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_MODE_SELECT) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //打印错误编码并返回
            Error_CallBack(5);
        }
    }
    
    //向I2C1总线广播EEPROM设备地址，并设置写方向
    I2C_Send7bitAddress(EEPROM_I2C,EEPROM_I2C_ADDR,I2C_Direction_Transmitter);
    
    //重置eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //等待EV6事件，直到检测成功
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //打印错误编码并返回
            Error_CallBack(6);
        }
    }
    
    //发送要读取的EEPROM存储单元格地址
    I2C_SendData(EEPROM_I2C,addr);
    
    //重置eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //等待EV8_2事件，直到检测成功
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_BYTE_TRANSMITTED) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //打印错误编码并返回
            Error_CallBack(7);
        }
    }
    
    //--------------------------------------------------------------------------------
    //产生第2次起始信号
    I2C_GenerateSTART(EEPROM_I2C,ENABLE);
    
    //重置eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //等待EV5事件，直到检测成功
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_MODE_SELECT) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //打印错误编码并返回
            Error_CallBack(8);
        }
    }
    
    /***************注意这里该是读方向了，具体见AT24C02手册时序************/
    //向I2C1总线广播EEPROM设备地址，并设置读方向
    I2C_Send7bitAddress(EEPROM_I2C,EEPROM_I2C_ADDR,I2C_Direction_Receiver);
    
    //重置eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //等待EV6事件(与前面的EV6事件不同)，直到检测成功
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //打印错误编码并返回
            Error_CallBack(9);
        }
    }
    
    
    //STM32做出非应答信号:No ACK，表示不想再接收EEPROM的数据了
    
    /*这里做出非应答信号应该放在接收数据前面，这是因为，接收数据函数是将数据从I2C外设的DR寄存器中读出，
     *如果I2C_AcknowledgeConfig放在I2C_ReceiveData函数之后，相当于多接收了一个数据。
     *具体过程是因为：由于我们在I2C_Init中将STM32默认配置了自动ACK(I2C_Ack_Enable)，并且STM32的速度很快，所以当我们把设备地址(第2次)发送出去并设为读方向后，
     *               于是EEPROM向STM32传输一字节数据，然而这个字节数据还没有存储到STM32 I2C外设的DR寄存器,STM32就已经自动应答了ACK，
     *               STM32读取DR寄存器的数据保存到data里，但是由于STM32自动应答了ACK，EEPROM会继续向STM32传输一字节数据，然后STM32发出非应答信号No ACK,
     *               此时，EEPROM后面停止向STM32传输数据，但是第2个字节数据已经发送到STM32了。于是导致数据紊乱。
     *所以，正确做法是将I2C_AcknowledgeConfig放在I2C_ReceiveData函数之前，那么当我们把设备地址(第2次)发送出去并设为读方向后，于是EEPROM向STM32传输一字节数据，
     *               然后立刻做出非应答信号 No ACK,那么STM32就不会再自动应答了，然后等待EV7事件(DR寄存器已经接收到一个字节数据了，DR非空)，然后才读取数据。
     */
    I2C_AcknowledgeConfig(EEPROM_I2C,DISABLE);
    
    //重置eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //等待EV7事件，直到检测成功
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_BYTE_RECEIVED) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //打印错误编码并返回
            Error_CallBack(10);
        }
    }
    
    //接收EEPROM传回的数据
    *data = I2C_ReceiveData(EEPROM_I2C); 
    
    //产生结束信号
    I2C_GenerateSTOP(EEPROM_I2C,ENABLE);

    return 0;
}


//addr:要写入的存储单元首地址
//data:要写入的数据的指针 
//size:要写入多少个数据(size小于等于8)
//return :0表示正常，非0为失败
uint8_t EEPROM_Page_Write(uint8_t addr ,uint8_t * data ,uint8_t size)
{    
    //产生起始信号
    I2C_GenerateSTART(EEPROM_I2C,ENABLE);
    
    //重置eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //等待EV5事件，直到检测成功
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_MODE_SELECT) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //打印错误编码并返回
            Error_CallBack(12);
        }
    }
    
    //向I2C1总线广播EEPROM设备地址，并设置写方向
    I2C_Send7bitAddress(EEPROM_I2C,EEPROM_I2C_ADDR,I2C_Direction_Transmitter);
    
    //重置eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //等待EV6事件，直到检测成功
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //打印错误编码并返回
            Error_CallBack(13);
        }
    }
    
    //发送要写入的EEPROM存储单元格地址
    I2C_SendData(EEPROM_I2C,addr);
    
    //重置eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //等待EV8_2事件，直到检测成功
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_BYTE_TRANSMITTED) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //打印错误编码并返回
            Error_CallBack(14);
        }
    }
    while(size--)
    {
        //发送要写入的数据
        I2C_SendData(EEPROM_I2C,*data);
        
        //重置eeprom_count_wait
        eeprom_count_wait = EEPROM_TIME_OUT;
        //等待EV8_2事件，直到检测成功
        while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_BYTE_TRANSMITTED) != SUCCESS)
        {
            eeprom_count_wait --;
            if(eeprom_count_wait == 0)
            {
                //打印错误编码并返回
                Error_CallBack(15);
            }
        }
        
        data++;
    }
    
    //产生结束信号
    I2C_GenerateSTOP(EEPROM_I2C,ENABLE);

    //等待EEPROM写入时序完成
    return Wait_For_Standby();
}



//addr:要读取的EEPROM单元格首地址
//*data:读取到的数据指针，读取到的数据存储在这个指针所指向的地方(即地址).
//size:要读取多少个数据(小于256)
//return :0表示正常，非0为失败
uint8_t EEPROM_Sequential_Read(uint8_t addr , uint8_t *data ,uint16_t size)
{
    //产生起始信号
    I2C_GenerateSTART(EEPROM_I2C,ENABLE);
    
    //重置eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //等待EV5事件，直到检测成功
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_MODE_SELECT) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //打印错误编码并返回
            Error_CallBack(16);
        }
    }
    
    //向I2C1总线广播EEPROM设备地址，并设置写方向
    I2C_Send7bitAddress(EEPROM_I2C,EEPROM_I2C_ADDR,I2C_Direction_Transmitter);
    
    //重置eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //等待EV6事件，直到检测成功
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //打印错误编码并返回
            Error_CallBack(17);
        }
    }
    
    //发送要读取的EEPROM存储单元格地址
    I2C_SendData(EEPROM_I2C,addr);
    
    //重置eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //等待EV8_2事件，直到检测成功
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_BYTE_TRANSMITTED) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //打印错误编码并返回
            Error_CallBack(18);
        }
    }
    
    //--------------------------------------------------------------------------------
    //产生第2次起始信号
    I2C_GenerateSTART(EEPROM_I2C,ENABLE);
    
    //重置eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //等待EV5事件，直到检测成功
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_MODE_SELECT) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //打印错误编码并返回
            Error_CallBack(19);
        }
    }
    
    /***************注意这里该是读方向了，具体见AT24C02手册时序************/
    //向I2C1总线广播EEPROM设备地址，并设置读方向
    I2C_Send7bitAddress(EEPROM_I2C,EEPROM_I2C_ADDR,I2C_Direction_Receiver);
    
    //重置eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //等待EV6事件(与前面的EV6事件不同)，直到检测成功
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //打印错误编码并返回
            Error_CallBack(20);
        }
    }
    
    while(size--)
    {
        //如果接收的是最后一个数据了，就做出非应答信号
        if(size == 0)
        {
            //STM32做出非应答信号:No ACK
            I2C_AcknowledgeConfig(EEPROM_I2C,DISABLE);
        }
        else
        {
            //STM32做出应答信号: ACK
            I2C_AcknowledgeConfig(EEPROM_I2C,ENABLE);
        }
        
        //重置eeprom_count_wait
        eeprom_count_wait = EEPROM_TIME_OUT;
        //等待EV7事件，直到检测成功
        while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_BYTE_RECEIVED) != SUCCESS)
        {
            eeprom_count_wait --;
            if(eeprom_count_wait == 0)
            {
                //打印错误编码并返回
                Error_CallBack(21);
            }
        }
        
        //接收EEPROM传回的数据
        *data = I2C_ReceiveData(EEPROM_I2C);
        
        data++;
    }
    
    //产生结束信号
    I2C_GenerateSTOP(EEPROM_I2C,ENABLE);

    return 0;
}



//addr:要写入的存储单元首地址(地址需要对齐，为8的倍数)
//data:要写入的数据的指针 
//size:要写入多少个数据(小于256)
//return :0表示正常，非0为失败
uint8_t EEPROM_Sequential_Write(uint8_t addr ,uint8_t * data ,uint16_t size)
{
    uint8_t num_of_page = size / EEPROM_PAGE_SIZE;
    uint8_t num_of_byte = size % EEPROM_PAGE_SIZE;
    
    while(num_of_page--)
    {
        //调用页写入函数
        EEPROM_Page_Write(addr,data,EEPROM_PAGE_SIZE);
        
        //等待写入完成
        Wait_For_Standby();
        
        addr += EEPROM_PAGE_SIZE;
        
        data += EEPROM_PAGE_SIZE;
    }
    
    //调用页写入函数
    EEPROM_Page_Write(addr,data,num_of_byte);
    
    //等待写入完成
    Wait_For_Standby();
    
    return 0;
}


//addr:要写入的存储单元首地址(地址任意，无需对齐)
//data:要写入的数据的指针 
//size:要写入多少个数据(小于256)
//return :0表示正常，非0为失败
uint8_t EEPROM_Sequential_Write_Update(uint8_t addr ,uint8_t * data ,uint16_t size)
{
    uint8_t num_of_single_addr = addr % EEPROM_PAGE_SIZE;
    
    if(num_of_single_addr == 0)    //addr对齐
    {
        uint8_t num_of_page = size / EEPROM_PAGE_SIZE;
        uint8_t num_of_byte = size % EEPROM_PAGE_SIZE;

        while(num_of_page--)
        {
            //调用页写入函数
            EEPROM_Page_Write(addr,data,EEPROM_PAGE_SIZE);
            
            //等待写入完成
            Wait_For_Standby();
            
            addr += EEPROM_PAGE_SIZE;
            
            data += EEPROM_PAGE_SIZE;
        }
        
        //调用页写入函数
        EEPROM_Page_Write(addr,data,num_of_byte);
        
        //等待写入完成
        Wait_For_Standby();
    }
    else    //addr不对齐
    {
        //第一次要写入的字节数
        uint8_t num_of_first_size  = EEPROM_PAGE_SIZE - num_of_single_addr;
        
        uint8_t surplus = size - num_of_first_size;
        uint8_t num_of_page = surplus / EEPROM_PAGE_SIZE;
        uint8_t num_of_byte = surplus % EEPROM_PAGE_SIZE;

        //写入第一次要写入的数据
        EEPROM_Page_Write(addr,data,num_of_first_size);
        
        //等待写入完成
        Wait_For_Standby();
        
        addr += num_of_first_size;
        
        data += num_of_first_size;
        
        //现在地址已经对齐
        //循环写入整数个页的数据
        while(num_of_page--)
        {
            //调用页写入函数
            EEPROM_Page_Write(addr,data,EEPROM_PAGE_SIZE);
            
            //等待写入完成
            Wait_For_Standby();
            
            addr += EEPROM_PAGE_SIZE;
            
            data += EEPROM_PAGE_SIZE;
        }
        
        //写入剩余的剩余
        //调用页写入函数
        EEPROM_Page_Write(addr,data,num_of_byte);
        
        //等待写入完成
        Wait_For_Standby();
    }

    return 0;
}


//等待EEPROM内部写入操作完成
//通过检测EEPROM对STM32发出的寻址信号的响应，达到等待的目的。
//return :0表示正常等待写入完成，非0为失败
uint8_t Wait_For_Standby(void)
{
    uint32_t eeprom_count_wait_write_complete = 0x00000FFF;
    while(eeprom_count_wait_write_complete--)
    {
        
        //产生起始信号
        I2C_GenerateSTART(EEPROM_I2C,ENABLE);
        //重置eeprom_count_wait
        eeprom_count_wait = EEPROM_TIME_OUT;
        //等待EV5事件，直到检测成功
        while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_MODE_SELECT) != SUCCESS)
        {
            eeprom_count_wait --;
            if(eeprom_count_wait == 0)
            {
                //打印错误编码并返回
                Error_CallBack(11);
            }
        }
        
        //向I2C1总线广播EEPROM设备地址，并设置写方向
        I2C_Send7bitAddress(EEPROM_I2C,EEPROM_I2C_ADDR,I2C_Direction_Transmitter);
        
        //重置eeprom_count_wait
        eeprom_count_wait = EEPROM_TIME_OUT;
        //等待EV6事件eeprom_count_wait时间，如果期间检测到EV6事件成功，说明内部写时序完成，就返回，跳出等待函数。
        //                           否则如果eeprom_count_wait时间内还没有检测到EV6事件成功，就结束这个小循环，继续大循环再发出起始信号发送设备地址，看EEPROM是否有响应。
        while(eeprom_count_wait--)
        {
            if(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) == SUCCESS)
            {
                //退出前停止本次通讯
                I2C_GenerateSTOP(EEPROM_I2C,ENABLE);
                return 0;
            }
        }
    }
    
    //退出前停止本次通讯
    I2C_GenerateSTOP(EEPROM_I2C,ENABLE);
    return 1;
}

//code：错误编码
static uint8_t Error_CallBack(uint8_t code)
{
    printf("\r\nI2C1 error occurred , code = %d",code);
    return code;
}


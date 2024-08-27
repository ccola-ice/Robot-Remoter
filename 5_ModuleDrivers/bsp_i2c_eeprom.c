#include "bsp_i2c_eeprom.h"

uint32_t eeprom_count_wait = EEPROM_TIME_OUT;

 /**
  * @brief  EEPROM I2C1 GPIO��ʼ��
  * @param  ��
  * @retval ��
  */
static void EEPROM_I2C_GPIO_Config(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    
    /* ʹ��I2C1��ӦGPIO����ʱ�� */
    EEPROM_I2C_SDA_GPIO_CLK_INIT( EEPROM_I2C_SDA_GPIO_CLK, ENABLE);
    EEPROM_I2C_SCL_GPIO_CLK_INIT( EEPROM_I2C_SCL_GPIO_CLK, ENABLE);

    /* ���� PXx���� �� I2C SCL���� F4XXϵ������(��Ĭ�����������û��������Ӧ���ţ�Ҫͨ���˽��������ӵ���Ӧ����) */
    GPIO_PinAFConfig(EEPROM_I2C_SDA_GPIO_PORT,EEPROM_I2C_SDA_SOURCE,EEPROM_I2C_SDA_AF);
    GPIO_PinAFConfig(EEPROM_I2C_SCL_GPIO_PORT,EEPROM_I2C_SCL_SOURCE,EEPROM_I2C_SCL_AF);

    /* I2C1 SDA GPIO��ʼ�� */
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;      //��������Ϊ��©���
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;        //���ù���
    GPIO_InitStructure.GPIO_Pin = EEPROM_I2C_SDA_GPIO_PIN  ;
    GPIO_Init(EEPROM_I2C_SDA_GPIO_PORT, &GPIO_InitStructure);
    /* I2C1 SCL GPIO��ʼ�� */
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Pin = EEPROM_I2C_SCL_GPIO_PIN  ;
    GPIO_Init(EEPROM_I2C_SCL_GPIO_PORT, &GPIO_InitStructure);
}


 /**
  * @brief  EEPROM I2C1 ����ģʽ��ʼ��
  * @param  ��
  * @retval ��
  */
static void EEPROM_I2C_MODE_Config(void)
{
    I2C_InitTypeDef   I2C_InitStructure;
    
    /* ʹ��I2C1ʱ�� */
    EEPROM_I2C_CLK_INIT( EEPROM_I2C_CLK, ENABLE);
    
    /* I2C1 ��ʼ�� */
    I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_InitStructure.I2C_ClockSpeed = I2C_SPEED;
    I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
    I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStructure.I2C_OwnAddress1 = STM32_I2C_OWN_ADDR;    //STM32��I2C�豸��ַ��
    
    I2C_Init(EEPROM_I2C,&I2C_InitStructure);

    /* ʹ�� I2C1 */
    I2C_Cmd(EEPROM_I2C,ENABLE);
    
    I2C_AcknowledgeConfig(EEPROM_I2C, ENABLE);
}

//EEPROM I2C��ʼ��
void EEPROM_I2C_Init(void)
{
    EEPROM_I2C_GPIO_Config();
    EEPROM_I2C_MODE_Config();
}


//addr:Ҫд��Ĵ洢��Ԫ��ַ
//data:Ҫд�������
//note:ֻ��д��һ���ֽ�
//return :0��ʾ��������0Ϊʧ��
uint8_t EEPROM_Byte_Write(uint8_t addr ,uint8_t data)
{
    //(�й��¼����������Ұ���ֲ�� 24.2.3ͨѶ���� )
    
    //������ʼ�ź�
    I2C_GenerateSTART(EEPROM_I2C,ENABLE);
    
    //����eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //�ȴ�EV5�¼���ֱ�����ɹ�
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_MODE_SELECT) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //��ӡ������벢����
            Error_CallBack(1);
        }
    }
    
    //��I2C1���߹㲥EEPROM�豸��ַ��������д����
    I2C_Send7bitAddress(EEPROM_I2C,EEPROM_I2C_ADDR,I2C_Direction_Transmitter);
    
    //����eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //�ȴ�EV6�¼���ֱ�����ɹ�
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //��ӡ������벢����
            Error_CallBack(2);
        }
    }
    
    //����Ҫд���EEPROM�洢��Ԫ���ַ
    I2C_SendData(EEPROM_I2C,addr);
    
    //����eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //�ȴ�EV8_2�¼���ֱ�����ɹ�
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_BYTE_TRANSMITTED) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //��ӡ������벢����
            Error_CallBack(3);
        }
    }
    
    //����Ҫд�������
    I2C_SendData(EEPROM_I2C,data);
    
    //����eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //�ȴ�EV8_2�¼���ֱ�����ɹ�
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_BYTE_TRANSMITTED) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //��ӡ������벢����
            Error_CallBack(4);
        }
    }
    
    //���������ź�
    I2C_GenerateSTOP(EEPROM_I2C,ENABLE);

    //�ȴ�EEPROMд��ʱ�����
    return Wait_For_Standby();
}


//addr:Ҫ��ȡ��EEPROM��Ԫ���ַ
//*data:��ȡ��������ָ�룬��ȡ�������ݴ洢�����ָ����ָ��ĵط�(����ַ).
//return :0��ʾ��������0Ϊʧ��
uint8_t EEPROM_Random_Read(uint8_t addr , uint8_t *data)
{
    //������ʼ�ź�
    I2C_GenerateSTART(EEPROM_I2C,ENABLE);
    
    //����eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //�ȴ�EV5�¼���ֱ�����ɹ�
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_MODE_SELECT) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //��ӡ������벢����
            Error_CallBack(5);
        }
    }
    
    //��I2C1���߹㲥EEPROM�豸��ַ��������д����
    I2C_Send7bitAddress(EEPROM_I2C,EEPROM_I2C_ADDR,I2C_Direction_Transmitter);
    
    //����eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //�ȴ�EV6�¼���ֱ�����ɹ�
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //��ӡ������벢����
            Error_CallBack(6);
        }
    }
    
    //����Ҫ��ȡ��EEPROM�洢��Ԫ���ַ
    I2C_SendData(EEPROM_I2C,addr);
    
    //����eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //�ȴ�EV8_2�¼���ֱ�����ɹ�
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_BYTE_TRANSMITTED) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //��ӡ������벢����
            Error_CallBack(7);
        }
    }
    
    //--------------------------------------------------------------------------------
    //������2����ʼ�ź�
    I2C_GenerateSTART(EEPROM_I2C,ENABLE);
    
    //����eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //�ȴ�EV5�¼���ֱ�����ɹ�
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_MODE_SELECT) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //��ӡ������벢����
            Error_CallBack(8);
        }
    }
    
    /***************ע��������Ƕ������ˣ������AT24C02�ֲ�ʱ��************/
    //��I2C1���߹㲥EEPROM�豸��ַ�������ö�����
    I2C_Send7bitAddress(EEPROM_I2C,EEPROM_I2C_ADDR,I2C_Direction_Receiver);
    
    //����eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //�ȴ�EV6�¼�(��ǰ���EV6�¼���ͬ)��ֱ�����ɹ�
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //��ӡ������벢����
            Error_CallBack(9);
        }
    }
    
    
    //STM32������Ӧ���ź�:No ACK����ʾ�����ٽ���EEPROM��������
    
    /*����������Ӧ���ź�Ӧ�÷��ڽ�������ǰ�棬������Ϊ���������ݺ����ǽ����ݴ�I2C�����DR�Ĵ����ж�����
     *���I2C_AcknowledgeConfig����I2C_ReceiveData����֮���൱�ڶ������һ�����ݡ�
     *�����������Ϊ������������I2C_Init�н�STM32Ĭ���������Զ�ACK(I2C_Ack_Enable)������STM32���ٶȺܿ죬���Ե����ǰ��豸��ַ(��2��)���ͳ�ȥ����Ϊ�������
     *               ����EEPROM��STM32����һ�ֽ����ݣ�Ȼ������ֽ����ݻ�û�д洢��STM32 I2C�����DR�Ĵ���,STM32���Ѿ��Զ�Ӧ����ACK��
     *               STM32��ȡDR�Ĵ��������ݱ��浽data���������STM32�Զ�Ӧ����ACK��EEPROM�������STM32����һ�ֽ����ݣ�Ȼ��STM32������Ӧ���ź�No ACK,
     *               ��ʱ��EEPROM����ֹͣ��STM32�������ݣ����ǵ�2���ֽ������Ѿ����͵�STM32�ˡ����ǵ����������ҡ�
     *���ԣ���ȷ�����ǽ�I2C_AcknowledgeConfig����I2C_ReceiveData����֮ǰ����ô�����ǰ��豸��ַ(��2��)���ͳ�ȥ����Ϊ�����������EEPROM��STM32����һ�ֽ����ݣ�
     *               Ȼ������������Ӧ���ź� No ACK,��ôSTM32�Ͳ������Զ�Ӧ���ˣ�Ȼ��ȴ�EV7�¼�(DR�Ĵ����Ѿ����յ�һ���ֽ������ˣ�DR�ǿ�)��Ȼ��Ŷ�ȡ���ݡ�
     */
    I2C_AcknowledgeConfig(EEPROM_I2C,DISABLE);
    
    //����eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //�ȴ�EV7�¼���ֱ�����ɹ�
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_BYTE_RECEIVED) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //��ӡ������벢����
            Error_CallBack(10);
        }
    }
    
    //����EEPROM���ص�����
    *data = I2C_ReceiveData(EEPROM_I2C); 
    
    //���������ź�
    I2C_GenerateSTOP(EEPROM_I2C,ENABLE);

    return 0;
}


//addr:Ҫд��Ĵ洢��Ԫ�׵�ַ
//data:Ҫд������ݵ�ָ�� 
//size:Ҫд����ٸ�����(sizeС�ڵ���8)
//return :0��ʾ��������0Ϊʧ��
uint8_t EEPROM_Page_Write(uint8_t addr ,uint8_t * data ,uint8_t size)
{    
    //������ʼ�ź�
    I2C_GenerateSTART(EEPROM_I2C,ENABLE);
    
    //����eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //�ȴ�EV5�¼���ֱ�����ɹ�
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_MODE_SELECT) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //��ӡ������벢����
            Error_CallBack(12);
        }
    }
    
    //��I2C1���߹㲥EEPROM�豸��ַ��������д����
    I2C_Send7bitAddress(EEPROM_I2C,EEPROM_I2C_ADDR,I2C_Direction_Transmitter);
    
    //����eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //�ȴ�EV6�¼���ֱ�����ɹ�
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //��ӡ������벢����
            Error_CallBack(13);
        }
    }
    
    //����Ҫд���EEPROM�洢��Ԫ���ַ
    I2C_SendData(EEPROM_I2C,addr);
    
    //����eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //�ȴ�EV8_2�¼���ֱ�����ɹ�
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_BYTE_TRANSMITTED) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //��ӡ������벢����
            Error_CallBack(14);
        }
    }
    while(size--)
    {
        //����Ҫд�������
        I2C_SendData(EEPROM_I2C,*data);
        
        //����eeprom_count_wait
        eeprom_count_wait = EEPROM_TIME_OUT;
        //�ȴ�EV8_2�¼���ֱ�����ɹ�
        while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_BYTE_TRANSMITTED) != SUCCESS)
        {
            eeprom_count_wait --;
            if(eeprom_count_wait == 0)
            {
                //��ӡ������벢����
                Error_CallBack(15);
            }
        }
        
        data++;
    }
    
    //���������ź�
    I2C_GenerateSTOP(EEPROM_I2C,ENABLE);

    //�ȴ�EEPROMд��ʱ�����
    return Wait_For_Standby();
}



//addr:Ҫ��ȡ��EEPROM��Ԫ���׵�ַ
//*data:��ȡ��������ָ�룬��ȡ�������ݴ洢�����ָ����ָ��ĵط�(����ַ).
//size:Ҫ��ȡ���ٸ�����(С��256)
//return :0��ʾ��������0Ϊʧ��
uint8_t EEPROM_Sequential_Read(uint8_t addr , uint8_t *data ,uint16_t size)
{
    //������ʼ�ź�
    I2C_GenerateSTART(EEPROM_I2C,ENABLE);
    
    //����eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //�ȴ�EV5�¼���ֱ�����ɹ�
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_MODE_SELECT) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //��ӡ������벢����
            Error_CallBack(16);
        }
    }
    
    //��I2C1���߹㲥EEPROM�豸��ַ��������д����
    I2C_Send7bitAddress(EEPROM_I2C,EEPROM_I2C_ADDR,I2C_Direction_Transmitter);
    
    //����eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //�ȴ�EV6�¼���ֱ�����ɹ�
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //��ӡ������벢����
            Error_CallBack(17);
        }
    }
    
    //����Ҫ��ȡ��EEPROM�洢��Ԫ���ַ
    I2C_SendData(EEPROM_I2C,addr);
    
    //����eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //�ȴ�EV8_2�¼���ֱ�����ɹ�
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_BYTE_TRANSMITTED) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //��ӡ������벢����
            Error_CallBack(18);
        }
    }
    
    //--------------------------------------------------------------------------------
    //������2����ʼ�ź�
    I2C_GenerateSTART(EEPROM_I2C,ENABLE);
    
    //����eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //�ȴ�EV5�¼���ֱ�����ɹ�
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_MODE_SELECT) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //��ӡ������벢����
            Error_CallBack(19);
        }
    }
    
    /***************ע��������Ƕ������ˣ������AT24C02�ֲ�ʱ��************/
    //��I2C1���߹㲥EEPROM�豸��ַ�������ö�����
    I2C_Send7bitAddress(EEPROM_I2C,EEPROM_I2C_ADDR,I2C_Direction_Receiver);
    
    //����eeprom_count_wait
    eeprom_count_wait = EEPROM_TIME_OUT;
    //�ȴ�EV6�¼�(��ǰ���EV6�¼���ͬ)��ֱ�����ɹ�
    while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED) != SUCCESS)
    {
        eeprom_count_wait --;
        if(eeprom_count_wait == 0)
        {
            //��ӡ������벢����
            Error_CallBack(20);
        }
    }
    
    while(size--)
    {
        //������յ������һ�������ˣ���������Ӧ���ź�
        if(size == 0)
        {
            //STM32������Ӧ���ź�:No ACK
            I2C_AcknowledgeConfig(EEPROM_I2C,DISABLE);
        }
        else
        {
            //STM32����Ӧ���ź�: ACK
            I2C_AcknowledgeConfig(EEPROM_I2C,ENABLE);
        }
        
        //����eeprom_count_wait
        eeprom_count_wait = EEPROM_TIME_OUT;
        //�ȴ�EV7�¼���ֱ�����ɹ�
        while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_BYTE_RECEIVED) != SUCCESS)
        {
            eeprom_count_wait --;
            if(eeprom_count_wait == 0)
            {
                //��ӡ������벢����
                Error_CallBack(21);
            }
        }
        
        //����EEPROM���ص�����
        *data = I2C_ReceiveData(EEPROM_I2C);
        
        data++;
    }
    
    //���������ź�
    I2C_GenerateSTOP(EEPROM_I2C,ENABLE);

    return 0;
}



//addr:Ҫд��Ĵ洢��Ԫ�׵�ַ(��ַ��Ҫ���룬Ϊ8�ı���)
//data:Ҫд������ݵ�ָ�� 
//size:Ҫд����ٸ�����(С��256)
//return :0��ʾ��������0Ϊʧ��
uint8_t EEPROM_Sequential_Write(uint8_t addr ,uint8_t * data ,uint16_t size)
{
    uint8_t num_of_page = size / EEPROM_PAGE_SIZE;
    uint8_t num_of_byte = size % EEPROM_PAGE_SIZE;
    
    while(num_of_page--)
    {
        //����ҳд�뺯��
        EEPROM_Page_Write(addr,data,EEPROM_PAGE_SIZE);
        
        //�ȴ�д�����
        Wait_For_Standby();
        
        addr += EEPROM_PAGE_SIZE;
        
        data += EEPROM_PAGE_SIZE;
    }
    
    //����ҳд�뺯��
    EEPROM_Page_Write(addr,data,num_of_byte);
    
    //�ȴ�д�����
    Wait_For_Standby();
    
    return 0;
}


//addr:Ҫд��Ĵ洢��Ԫ�׵�ַ(��ַ���⣬�������)
//data:Ҫд������ݵ�ָ�� 
//size:Ҫд����ٸ�����(С��256)
//return :0��ʾ��������0Ϊʧ��
uint8_t EEPROM_Sequential_Write_Update(uint8_t addr ,uint8_t * data ,uint16_t size)
{
    uint8_t num_of_single_addr = addr % EEPROM_PAGE_SIZE;
    
    if(num_of_single_addr == 0)    //addr����
    {
        uint8_t num_of_page = size / EEPROM_PAGE_SIZE;
        uint8_t num_of_byte = size % EEPROM_PAGE_SIZE;

        while(num_of_page--)
        {
            //����ҳд�뺯��
            EEPROM_Page_Write(addr,data,EEPROM_PAGE_SIZE);
            
            //�ȴ�д�����
            Wait_For_Standby();
            
            addr += EEPROM_PAGE_SIZE;
            
            data += EEPROM_PAGE_SIZE;
        }
        
        //����ҳд�뺯��
        EEPROM_Page_Write(addr,data,num_of_byte);
        
        //�ȴ�д�����
        Wait_For_Standby();
    }
    else    //addr������
    {
        //��һ��Ҫд����ֽ���
        uint8_t num_of_first_size  = EEPROM_PAGE_SIZE - num_of_single_addr;
        
        uint8_t surplus = size - num_of_first_size;
        uint8_t num_of_page = surplus / EEPROM_PAGE_SIZE;
        uint8_t num_of_byte = surplus % EEPROM_PAGE_SIZE;

        //д���һ��Ҫд�������
        EEPROM_Page_Write(addr,data,num_of_first_size);
        
        //�ȴ�д�����
        Wait_For_Standby();
        
        addr += num_of_first_size;
        
        data += num_of_first_size;
        
        //���ڵ�ַ�Ѿ�����
        //ѭ��д��������ҳ������
        while(num_of_page--)
        {
            //����ҳд�뺯��
            EEPROM_Page_Write(addr,data,EEPROM_PAGE_SIZE);
            
            //�ȴ�д�����
            Wait_For_Standby();
            
            addr += EEPROM_PAGE_SIZE;
            
            data += EEPROM_PAGE_SIZE;
        }
        
        //д��ʣ���ʣ��
        //����ҳд�뺯��
        EEPROM_Page_Write(addr,data,num_of_byte);
        
        //�ȴ�д�����
        Wait_For_Standby();
    }

    return 0;
}


//�ȴ�EEPROM�ڲ�д��������
//ͨ�����EEPROM��STM32������Ѱַ�źŵ���Ӧ���ﵽ�ȴ���Ŀ�ġ�
//return :0��ʾ�����ȴ�д����ɣ���0Ϊʧ��
uint8_t Wait_For_Standby(void)
{
    uint32_t eeprom_count_wait_write_complete = 0x00000FFF;
    while(eeprom_count_wait_write_complete--)
    {
        
        //������ʼ�ź�
        I2C_GenerateSTART(EEPROM_I2C,ENABLE);
        //����eeprom_count_wait
        eeprom_count_wait = EEPROM_TIME_OUT;
        //�ȴ�EV5�¼���ֱ�����ɹ�
        while(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_MODE_SELECT) != SUCCESS)
        {
            eeprom_count_wait --;
            if(eeprom_count_wait == 0)
            {
                //��ӡ������벢����
                Error_CallBack(11);
            }
        }
        
        //��I2C1���߹㲥EEPROM�豸��ַ��������д����
        I2C_Send7bitAddress(EEPROM_I2C,EEPROM_I2C_ADDR,I2C_Direction_Transmitter);
        
        //����eeprom_count_wait
        eeprom_count_wait = EEPROM_TIME_OUT;
        //�ȴ�EV6�¼�eeprom_count_waitʱ�䣬����ڼ��⵽EV6�¼��ɹ���˵���ڲ�дʱ����ɣ��ͷ��أ������ȴ�������
        //                           �������eeprom_count_waitʱ���ڻ�û�м�⵽EV6�¼��ɹ����ͽ������Сѭ����������ѭ���ٷ�����ʼ�źŷ����豸��ַ����EEPROM�Ƿ�����Ӧ��
        while(eeprom_count_wait--)
        {
            if(I2C_CheckEvent(EEPROM_I2C,I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) == SUCCESS)
            {
                //�˳�ǰֹͣ����ͨѶ
                I2C_GenerateSTOP(EEPROM_I2C,ENABLE);
                return 0;
            }
        }
    }
    
    //�˳�ǰֹͣ����ͨѶ
    I2C_GenerateSTOP(EEPROM_I2C,ENABLE);
    return 1;
}

//code���������
static uint8_t Error_CallBack(uint8_t code)
{
    printf("\r\nI2C1 error occurred , code = %d",code);
    return code;
}


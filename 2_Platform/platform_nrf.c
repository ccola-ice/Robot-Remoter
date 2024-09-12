#include "platform_nrf.h"

uint8_t nrf_stat;// 用于判断接收/发送状态
uint8_t i;
uint8_t rxbuf[32]={0};			        // 接收缓冲
uint8_t txbuf[32]={0};	   				// 发送缓冲

uint16_t MQ_VALUE;
uint16_t LIGHT_VALUE;
float MQ_VALUE_Converted;
float LIGHT_VALUE_Converted;

//无线模块检测
void nrf24l01_check(void)
{
    /*检测 NRF 模块与 MCU 的连接*/
    nrf_stat = NRF_Check(); 

    /*判断连接状态*/  
    if(nrf_stat == SUCCESS)	   
        printf("\r\nNRF与MCU连接成功！Remoter持续接收数据\r\n");
        return 0;  
    else	  
        printf("\r\nNRF与MCU连接失败! 请重新检查接线\r\n");
        return 1;
}

//无线模块接收数据
void nrf24l01_receive(void)
{
    NRF_RX_Mode();     //进入接收模式

    /* 等待 NRF1 接收数据 */
    nrf_stat = NRF_Rx_Dat(rxbuf);

    /* 判断接收状态 */
    if(nrf_stat == RX_DR)
    {
        printf("接收数据成功");
        for(i=0;i<6;i++)
        {	
            printf("\r\nRemoter NRF接收数据为：%.2f \r\n",(float)rxbuf[i]); 
        }
    }
    NRF_RX_Mode();
}

//无线模块发送数据
void nrf24l01_send(void)
{
	NRF_TX_Mode();     //进入发送模式
	
	/* 等待 NRF1 发送数据 */
    nrf_stat = NRF_Tx_Dat(txbuf);

    /* 判断发送状态 */
    if(nrf_stat == TX_DS)
    {
        printf("发送数据成功\n\r");
    }
	else 
		printf("发送数据失败\n\r");
}

void nrf24l01_show(void)
{
    MQ_VALUE = (rxbuf[2] << 8) | rxbuf[3];
    MQ_VALUE_Converted = (float)MQ_VALUE/4096*(float)3.3;
    LIGHT_VALUE = (rxbuf[4] << 8) | rxbuf[5];
    LIGHT_VALUE_Converted = (float)LIGHT_VALUE/4096*(float)3.3;
    
    printf("\r\n==========================NRF24L01无线接收测试开始=================================\r\n");
    printf("\r\n rxbuf[0] = %d",rxbuf[0]);
    printf("\r\n rxbuf[1] = %d",rxbuf[1]);
    printf("\r\n rxbuf[2] = %d",rxbuf[2]);
    printf("\r\n rxbuf[3] = %d",rxbuf[3]);
    printf("\r\n rxbuf[4] = %d",rxbuf[4]);
    printf("\r\n rxbuf[5] = %d",rxbuf[5]);
    
    printf("\r\n接收到的温度数据：%d\r\n",rxbuf[0]);
    printf("接收到的光强度数据：%d\r\n",rxbuf[1]);
    printf("接收到的二氧化碳浓度数据：%.3f\r\n",MQ_VALUE_Converted);
    printf("接收到的光亮度数据：%.3f\r\n",LIGHT_VALUE_Converted);    

}


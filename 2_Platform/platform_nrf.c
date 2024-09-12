#include "platform_nrf.h"

uint8_t nrf_stat;// �����жϽ���/����״̬
uint8_t i;
uint8_t rxbuf[32]={0};			        // ���ջ���
uint8_t txbuf[32]={0};	   				// ���ͻ���

uint16_t MQ_VALUE;
uint16_t LIGHT_VALUE;
float MQ_VALUE_Converted;
float LIGHT_VALUE_Converted;

//����ģ����
void nrf24l01_check(void)
{
    /*��� NRF ģ���� MCU ������*/
    nrf_stat = NRF_Check(); 

    /*�ж�����״̬*/  
    if(nrf_stat == SUCCESS)	   
        printf("\r\nNRF��MCU���ӳɹ���Remoter������������\r\n");
        return 0;  
    else	  
        printf("\r\nNRF��MCU����ʧ��! �����¼�����\r\n");
        return 1;
}

//����ģ���������
void nrf24l01_receive(void)
{
    NRF_RX_Mode();     //�������ģʽ

    /* �ȴ� NRF1 �������� */
    nrf_stat = NRF_Rx_Dat(rxbuf);

    /* �жϽ���״̬ */
    if(nrf_stat == RX_DR)
    {
        printf("�������ݳɹ�");
        for(i=0;i<6;i++)
        {	
            printf("\r\nRemoter NRF��������Ϊ��%.2f \r\n",(float)rxbuf[i]); 
        }
    }
    NRF_RX_Mode();
}

//����ģ�鷢������
void nrf24l01_send(void)
{
	NRF_TX_Mode();     //���뷢��ģʽ
	
	/* �ȴ� NRF1 �������� */
    nrf_stat = NRF_Tx_Dat(txbuf);

    /* �жϷ���״̬ */
    if(nrf_stat == TX_DS)
    {
        printf("�������ݳɹ�\n\r");
    }
	else 
		printf("��������ʧ��\n\r");
}

void nrf24l01_show(void)
{
    MQ_VALUE = (rxbuf[2] << 8) | rxbuf[3];
    MQ_VALUE_Converted = (float)MQ_VALUE/4096*(float)3.3;
    LIGHT_VALUE = (rxbuf[4] << 8) | rxbuf[5];
    LIGHT_VALUE_Converted = (float)LIGHT_VALUE/4096*(float)3.3;
    
    printf("\r\n==========================NRF24L01���߽��ղ��Կ�ʼ=================================\r\n");
    printf("\r\n rxbuf[0] = %d",rxbuf[0]);
    printf("\r\n rxbuf[1] = %d",rxbuf[1]);
    printf("\r\n rxbuf[2] = %d",rxbuf[2]);
    printf("\r\n rxbuf[3] = %d",rxbuf[3]);
    printf("\r\n rxbuf[4] = %d",rxbuf[4]);
    printf("\r\n rxbuf[5] = %d",rxbuf[5]);
    
    printf("\r\n���յ����¶����ݣ�%d\r\n",rxbuf[0]);
    printf("���յ��Ĺ�ǿ�����ݣ�%d\r\n",rxbuf[1]);
    printf("���յ��Ķ�����̼Ũ�����ݣ�%.3f\r\n",MQ_VALUE_Converted);
    printf("���յ��Ĺ��������ݣ�%.3f\r\n",LIGHT_VALUE_Converted);    

}


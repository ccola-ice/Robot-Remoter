#include "FATFS_SDCARD_test.h"
#include "bsp_Systick.h"

extern FATFS fs;                   	/* FatFs�ļ�ϵͳ���� */
extern FIL fnew_sdcard;				/* �ļ����� */
extern FRESULT res;                	/* �ļ�������� */
extern unsigned int fnum;			/* �ļ��ɹ���д���� */
extern unsigned char fatfs_read_buff[512];
extern unsigned char fatfs_write_buff[512];

void fatfs_sdcard_test(void)
{
	printf("===========================SD���ļ�ϵͳ��ֲ���Կ�ʼ=============================\n\r");
	
	//��SDCard�����ļ�ϵͳ���ļ�ϵͳ����ʱ���SDCard�豸��ʼ��
	res = f_mount(&fs,"0:",1);
	
	/*----------------------- ��ʽ������ ---------------------------*/  
	/* ���û���ļ�ϵͳ�͸�ʽ�������ļ�ϵͳ */
	if(res == FR_NO_FILESYSTEM)
	{
		printf("SD����û���ļ�ϵͳ���������и�ʽ��...\r\n");
		/* ��ʽ�� */
		res=f_mkfs("0:",0,0);							
		
		Delay_us(100);
		
		if(res == FR_OK)
		{
			printf("SD���ѳɹ���ʽ���ļ�ϵͳ.\r\n");
			/* ��ʽ������ȡ������ */
			res = f_mount(NULL,"0:",1);			
			/* ���¹���	*/			
			res = f_mount(&fs,"0:",1);
		}
		else
		{
			printf("��ʽ��ʧ��.\r\n");
			while(1);
		}
	}
	
	else if(res!=FR_OK)
	{
		printf("����SD�������ļ�ϵͳʧ�ܡ�(%d)\r\n",res);
		printf("��������ԭ��SD����ʼ�����ɹ���\r\n");
		while(1);
	}
	
	else
	{
		printf("���ļ�ϵͳ���سɹ������Խ��ж�д����\r\n");
	}
  
	/*----------------------- �ļ�ϵͳ���ԣ�д���� -----------------------------*/
	/* ���ļ�������ļ��������򴴽��� */
	printf("\r\n****** ���������ļ�д�����... ******\r\n");	
	res = f_open(&fnew_sdcard, "0:FatFs��д�����ļ�.txt",FA_CREATE_ALWAYS | FA_WRITE );
	if ( res == FR_OK )
	{
		printf("����/����FatFs��д�����ļ�.txt�ļ��ɹ������ļ�д�����ݡ�\r\n");
		/* ��ָ���洢������д�뵽�ļ��� */
		res=f_write(&fnew_sdcard,fatfs_write_buff,sizeof(fatfs_write_buff),&fnum);
		if(res==FR_OK)
		{	
			printf("���ļ�д��ɹ���д���ֽ����ݣ�%d\n",fnum);
			printf("�����ļ�д�������Ϊ��\r\n%s\r\n",fatfs_write_buff);
		}
		else
		{
			printf("�����ļ�д��ʧ�ܣ�(%d)\n",res);
		}    
		/* ���ٶ�д���ر��ļ� */
		f_close(&fnew_sdcard);
	}
	else
	{	
		printf("������/�����ļ�ʧ�ܡ�\r\n");
	}
	
	/*------------------- �ļ�ϵͳ���ԣ������� ------------------------------------*/
	printf("****** ���������ļ���ȡ����... ******\r\n");
	res = f_open(&fnew_sdcard, "0:FatFs��д�����ļ�.txt", FA_OPEN_EXISTING | FA_READ); 	 
	if(res == FR_OK)
	{
		printf("�����ļ��ɹ���\r\n");
		res = f_read(&fnew_sdcard, fatfs_read_buff, sizeof(fatfs_read_buff), &fnum); 
		if(res==FR_OK)
		{
			printf("���ļ���ȡ�ɹ�,�����ֽ����ݣ�%d\r\n",fnum);
			printf("����ȡ�õ��ļ�����Ϊ��\r\n%s \r\n", fatfs_read_buff);	
		}
		else
		{
			printf("�����ļ���ȡʧ�ܣ�(%d)\n",res);
		}		
	}
	else
	{
		printf("�������ļ�ʧ�ܡ�\r\n");
	}
	
	/* ���ٶ�д���ر��ļ� */
	f_close(&fnew_sdcard);	
  
	/* ����ʹ���ļ�ϵͳ��ȡ�������ļ�ϵͳ */
	f_mount(NULL,"0:",1);
	
	printf("===========================SD���ļ�ϵͳ��ֲ���Խ���=============================\n\r");
}


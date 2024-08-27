#include "FATFS_SDCARD_test.h"
#include "bsp_Systick.h"

extern FATFS fs;                   	/* FatFs文件系统对象 */
extern FIL fnew_sdcard;				/* 文件对象 */
extern FRESULT res;                	/* 文件操作结果 */
extern unsigned int fnum;			/* 文件成功读写数量 */
extern unsigned char fatfs_read_buff[512];
extern unsigned char fatfs_write_buff[512];

void fatfs_sdcard_test(void)
{
	printf("===========================SD卡文件系统移植测试开始=============================\n\r");
	
	//在SDCard挂载文件系统，文件系统挂载时会对SDCard设备初始化
	res = f_mount(&fs,"0:",1);
	
	/*----------------------- 格式化测试 ---------------------------*/  
	/* 如果没有文件系统就格式化创建文件系统 */
	if(res == FR_NO_FILESYSTEM)
	{
		printf("SD卡还没有文件系统，即将进行格式化...\r\n");
		/* 格式化 */
		res=f_mkfs("0:",0,0);							
		
		Delay_us(100);
		
		if(res == FR_OK)
		{
			printf("SD卡已成功格式化文件系统.\r\n");
			/* 格式化后，先取消挂载 */
			res = f_mount(NULL,"0:",1);			
			/* 重新挂载	*/			
			res = f_mount(&fs,"0:",1);
		}
		else
		{
			printf("格式化失败.\r\n");
			while(1);
		}
	}
	
	else if(res!=FR_OK)
	{
		printf("！！SD卡挂载文件系统失败。(%d)\r\n",res);
		printf("！！可能原因：SD卡初始化不成功。\r\n");
		while(1);
	}
	
	else
	{
		printf("》文件系统挂载成功，可以进行读写测试\r\n");
	}
  
	/*----------------------- 文件系统测试：写测试 -----------------------------*/
	/* 打开文件，如果文件不存在则创建它 */
	printf("\r\n****** 即将进行文件写入测试... ******\r\n");	
	res = f_open(&fnew_sdcard, "0:FatFs读写测试文件.txt",FA_CREATE_ALWAYS | FA_WRITE );
	if ( res == FR_OK )
	{
		printf("》打开/创建FatFs读写测试文件.txt文件成功，向文件写入数据。\r\n");
		/* 将指定存储区内容写入到文件内 */
		res=f_write(&fnew_sdcard,fatfs_write_buff,sizeof(fatfs_write_buff),&fnum);
		if(res==FR_OK)
		{	
			printf("》文件写入成功，写入字节数据：%d\n",fnum);
			printf("》向文件写入的数据为：\r\n%s\r\n",fatfs_write_buff);
		}
		else
		{
			printf("！！文件写入失败：(%d)\n",res);
		}    
		/* 不再读写，关闭文件 */
		f_close(&fnew_sdcard);
	}
	else
	{	
		printf("！！打开/创建文件失败。\r\n");
	}
	
	/*------------------- 文件系统测试：读测试 ------------------------------------*/
	printf("****** 即将进行文件读取测试... ******\r\n");
	res = f_open(&fnew_sdcard, "0:FatFs读写测试文件.txt", FA_OPEN_EXISTING | FA_READ); 	 
	if(res == FR_OK)
	{
		printf("》打开文件成功。\r\n");
		res = f_read(&fnew_sdcard, fatfs_read_buff, sizeof(fatfs_read_buff), &fnum); 
		if(res==FR_OK)
		{
			printf("》文件读取成功,读到字节数据：%d\r\n",fnum);
			printf("》读取得的文件数据为：\r\n%s \r\n", fatfs_read_buff);	
		}
		else
		{
			printf("！！文件读取失败：(%d)\n",res);
		}		
	}
	else
	{
		printf("！！打开文件失败。\r\n");
	}
	
	/* 不再读写，关闭文件 */
	f_close(&fnew_sdcard);	
  
	/* 不再使用文件系统，取消挂载文件系统 */
	f_mount(NULL,"0:",1);
	
	printf("===========================SD卡文件系统移植测试结束=============================\n\r");
}


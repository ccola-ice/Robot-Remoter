#include "FATFS_FLASH_test.h"
#include "bsp_Systick.h"

FATFS fs;                   /* FatFs�ļ�ϵͳ���� */
FIL fnew_flash;				/* �ļ����� */
FIL fnew_sdcard;			/* �ļ����� */
FRESULT res;                /* �ļ�������� */
unsigned int fnum;			/* �ļ��ɹ���д���� */
unsigned int bw;
unsigned int br;
unsigned char fatfs_read_buff[512] = {0};
unsigned char fatfs_write_buff[512] = "STM32F407 �����Ǹ������ӣ�SD���½��ļ�ϵͳ�����ļ�\r\n";
unsigned char fpath[100];                  /* ���浱ǰɨ��·�� */

//FLASH�ļ�ϵͳ����
void fatfs_flash_test(void)
{
    /***************************************Flash�ļ�ϵͳ��ֲ����*******************************************/
	printf("\n\r===========================FLASH�ļ�ϵͳ��ֲ���Կ�ʼ=============================\n\r");

	printf("\nFLASH�ļ�ϵͳ��ֲ����1: \n\r");
    
	//���ⲿSPI Flash�����ļ�ϵͳ���ļ�ϵͳ����ʱ���SPI�豸��ʼ��
    res = f_mount(&fs,"1:",1);
	
    printf("\r\nfmount res=%d",res);
	
	if(res == FR_NO_FILESYSTEM)
	{
		//��ʽ��
		res = f_mkfs("1:",0,0);
		printf("\r\nf_mkfs res=%d",res);
		
		//��ʽ������Ҫ���¹����ļ�ϵͳ
		res = f_mount(NULL,"1:",1);
		
		res = f_mount(&fs,"1:",1);		
	}
	
	res = f_open(&fnew_flash, "1:english.txt", FA_CREATE_ALWAYS|FA_READ|FA_WRITE);
	printf("\r\nf_open res=%d",res);
	res = f_write(&fnew_flash, "english name file,test okkkkkkkkk", strlen("english name file,test okkkkkkkkk") ,&bw);
	printf("\r\nf_write:res=%d len=%d bw=%d",res,strlen("english name file,test okkkkkkkkk"),bw);
	res = f_close(&fnew_flash);
	printf("\r\nf_close res=%d",res);
	
	
	res = f_open(&fnew_flash, "1:abcdefghijklmnopqrstuvwz.txt", FA_CREATE_ALWAYS|FA_READ|FA_WRITE);
	printf("\r\nf_open res=%d",res);
	res = f_write(&fnew_flash, "long name file", strlen("long name file") ,&bw);
	printf("\r\nf_write:res=%d len=%d bw=%d",res,strlen("long name file"),bw);
	res = f_close(&fnew_flash);
	printf("\r\nf_close res=%d",res);
	
	res = f_open(&fnew_flash, "1:�����ļ�.txt", FA_CREATE_ALWAYS|FA_READ|FA_WRITE);
	printf("\r\nf_open res=%d",res);
	res = f_write(&fnew_flash, fatfs_write_buff, sizeof(fatfs_write_buff) ,&bw);
	printf("\r\nf_write:in chinese file res=%d len=%d bw=%d",res,sizeof(fatfs_write_buff),bw);
	res = f_close(&fnew_flash);
	printf("\r\nf_close res=%d",res);
	
	
	res = f_open(&fnew_flash, "1:�����ļ�.txt", FA_OPEN_EXISTING|FA_READ);
	res = f_read(&fnew_flash, fatfs_read_buff, 40, &br);
	printf("\r\nf_read res=%d br=%d",res,br);
	fatfs_read_buff[br] = '\0';
	printf("\r\n��ȡ�����ļ�����:\r\n");
	printf("%s", fatfs_read_buff);
	res = f_close(&fnew_flash);
	
	res = f_open(&fnew_flash, "1:english.txt", FA_OPEN_EXISTING|FA_READ);
	res = f_read(&fnew_flash, fatfs_read_buff, 100, &br);
	printf("\r\nf_read res=%d br=%d",res,br);
	fatfs_read_buff[br] = '\0';
	printf("\r\n��ȡ�����ļ�����:");
	printf("%s\r\n", fatfs_read_buff);
	res = f_close(&fnew_flash);
}

void fatfs_flash_test2(void)
{
	printf("\r\nFLASH�ļ�ϵͳ��ֲ����2: \r\n");
	//���ⲿSPI Flash�����ļ�ϵͳ���ļ�ϵͳ����ʱ���SPI�豸��ʼ��
	res = f_mount(&fs,"1:",1);
	if(res!=FR_OK)
	{
		printf("\r\n�����ⲿFlash�����ļ�ϵͳʧ�ܡ�(%d)\r\n",res);
		printf("\r\n��������ԭ��SPI Flash��ʼ�����ɹ���\r\n");
		while(1);
	}
	else
	{
		printf("�ļ�ϵͳ���سɹ������Խ��в���\r\n");    
	}

	/* FatFs����ܲ��� */
	res = miscellaneous();
	
	printf("*************** �ļ���Ϣ��ȡ���� **************\r\n");
	res = file_check();

	printf("***************** �ļ�ɨ����� ****************\r\n");
	strcpy(fpath,"1:");
	scan_files(fpath);

	/* ����ʹ���ļ�ϵͳ��ȡ�������ļ�ϵͳ */
	f_mount(NULL,"1:",1);
	
	printf("===========================FLASH�ļ�ϵͳ��ֲ���Խ���=============================\n\r");
}

/* FatFs����ܲ��� */
FRESULT miscellaneous(void)
{
  DIR dir;
  FATFS *pfs;
  DWORD fre_clust, fre_sect, tot_sect;
  
  printf("*************** �豸��Ϣ��ȡ ***************\r\n");
  /* ��ȡ�豸��Ϣ�Ϳմش�С */
  res = f_getfree("1:", &fre_clust, &pfs);

  /* ����õ��ܵ����������Ϳ��������� */
  tot_sect = (pfs->n_fatent - 2) * pfs->csize;
  fre_sect = fre_clust * pfs->csize;

  /* ��ӡ��Ϣ(4096 �ֽ�/����) */
  printf("�豸�ܿռ䣺%10lu KB��\r\n���ÿռ䣺  %10lu KB��\r\n", tot_sect *4, fre_sect *4);
  
  printf("******** �ļ���λ�͸�ʽ��д�빦�ܲ��� ********\r\n");
  res = f_open(&fnew_flash, "1:FatFs��д�����ļ�.txt",FA_OPEN_ALWAYS|FA_WRITE|FA_READ );
	if ( res == FR_OK )
	{
    /*  �ļ���λ */
    res = f_lseek(&fnew_flash,f_size(&fnew_flash));
    if (res == FR_OK)
    {
      /* ��ʽ��д�룬������ʽ����printf���� */
      f_printf(&fnew_flash,"ԭ���ļ�d�������һ������\r\n");
      f_printf(&fnew_flash,"�豸�ܿռ䣺%10lu KB��\r\n���ÿռ䣺  %10lu KB��", tot_sect *4, fre_sect *4);
      /*  �ļ���λ���ļ���ʼλ�� */
      res = f_lseek(&fnew_flash,0);
      /* ��ȡ�ļ��������ݵ������� */
      res = f_read(&fnew_flash,fatfs_read_buff,f_size(&fnew_flash),&fnum);
      if(res == FR_OK)
      {
        printf("�ļ����ݣ�%s\r\n",fatfs_read_buff);
      }
    }
    f_close(&fnew_flash);    
    
    printf("********** Ŀ¼���������������ܲ��� **********\r\n");
    /* ���Դ�Ŀ¼ */
    res=f_opendir(&dir,"1:TestDir");
    if(res!=FR_OK)
    {
      /* ��Ŀ¼ʧ�ܣ��ʹ���Ŀ¼ */
      res=f_mkdir("1:TestDir");
    }
    else
    {
      /* ���Ŀ¼�Ѿ����ڣ��ر��� */
      res=f_closedir(&dir);
      /* ɾ���ļ� */
      f_unlink("1:TestDir/testdir.txt");
    }
    if(res==FR_OK)
    {
      /* ���������ƶ��ļ� */
      res=f_rename("1:FatFs��д�����ļ�.txt","1:TestDir/testdir.txt");      
    } 
	}
  else
  {
    printf("!! ���ļ�ʧ�ܣ�%d\n",res);
    printf("!! ������Ҫ�ٴ����С�FatFs��ֲ���д���ԡ�����\n");
  }
  return res;
}

FILINFO fno;
/**
  * �ļ���Ϣ��ȡ
  */
FRESULT file_check(void)
{
  /* ��ȡ�ļ���Ϣ */
  res=f_stat("1:TestDir/testdir.txt",&fno);
  if(res==FR_OK)
  {
    printf("��testdir.txt���ļ���Ϣ��\r\n");
    printf("�ļ���С: %ld(�ֽ�)\r\n", fno.fsize);
    printf("ʱ���: %u/%02u/%02u, %02u:%02u\r\n",
           (fno.fdate >> 9) + 1980, fno.fdate >> 5 & 15, fno.fdate & 31,fno.ftime >> 11, fno.ftime >> 5 & 63);
    printf("����: %c%c%c%c%c\n\r\n",
           (fno.fattrib & AM_DIR) ? 'D' : '-',      // ��һ��Ŀ¼
           (fno.fattrib & AM_RDO) ? 'R' : '-',      // ֻ���ļ�
           (fno.fattrib & AM_HID) ? 'H' : '-',      // �����ļ�
           (fno.fattrib & AM_SYS) ? 'S' : '-',      // ϵͳ�ļ�
           (fno.fattrib & AM_ARC) ? 'A' : '-');     // �����ļ�
  }
  return res;
}

/**
  * @brief  scan_files �ݹ�ɨ��FatFs�ڵ��ļ�
  * @param  path:��ʼɨ��·��
  * @retval result:�ļ�ϵͳ�ķ���ֵ
  */
FRESULT scan_files (char* path) 
{ 
  FRESULT res; 		//�����ڵݹ���̱��޸ĵı���������ȫ�ֱ���	
  FILINFO fno; 
  DIR dir; 
  int i;            
  char *fn;        // �ļ���	
	
#if _USE_LFN 
  /* ���ļ���֧�� */
  /* ����������Ҫ2���ֽڱ���һ�����֡�*/
  static char lfn[_MAX_LFN*2 + 1]; 	
  fno.lfname = lfn; 
  fno.lfsize = sizeof(lfn); 
#endif 
  //��Ŀ¼
  res = f_opendir(&dir, path); 
  if (res == FR_OK) 
	{ 
    i = strlen(path); 
    for (;;) 
		{ 
      //��ȡĿ¼�µ����ݣ��ٶ����Զ�����һ���ļ�
      res = f_readdir(&dir, &fno); 								
      //Ϊ��ʱ��ʾ������Ŀ��ȡ��ϣ�����
      if (res != FR_OK || fno.fname[0] == 0) break; 	
#if _USE_LFN 
      fn = *fno.lfname ? fno.lfname : fno.fname; 
#else 
      fn = fno.fname; 
#endif 
      //���ʾ��ǰĿ¼������			
      if (*fn == '.') continue; 	
      //Ŀ¼���ݹ��ȡ      
      if (fno.fattrib & AM_DIR)         
			{ 			
        //�ϳ�����Ŀ¼��        
        sprintf(&path[i], "/%s", fn); 		
        //�ݹ����         
        res = scan_files(path);	
        path[i] = 0;         
        //��ʧ�ܣ�����ѭ��        
        if (res != FR_OK) 
					break; 
      } 
			else 
			{ 
				printf("%s/%s\r\n", path, fn);								//����ļ���	
        /* ������������ȡ�ض���ʽ���ļ�·�� */        
      }//else
    } //for
  } 
  return res; 
}


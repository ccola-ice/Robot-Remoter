#include "SRAM_test.h"

//volatile uint8_t testvalue __attribute((at(0x6C000080)));
//volatile double testvalue_double __attribute((at(0x6C000100)));

/**
  * @brief  ����SRAM 
  * @param  None
  * @retval ��������1���쳣����0
  */
uint8_t sram_read_write_test(void)
{
    volatile uint8_t  * p_iner  = (uint8_t  *)INER_SRAM_ADDR;
	volatile uint8_t  * p_8		= (uint8_t  *)SRAM_BASE_ADDR;
	volatile uint16_t * p_16	= (uint16_t *)(SRAM_BASE_ADDR+10);
	volatile uint32_t * p_32	= (uint32_t *)(SRAM_BASE_ADDR+20);
	
	/*д�����ݼ�����*/
	uint32_t counter=0;

	/* 8λ������ */
	uint8_t ubWritedata_8b = 0, ubReaddata_8b = 0;  

	/* 16λ������ */
	uint16_t uhWritedata_16b = 0, uhReaddata_16b = 0; 
	
//	testvalue = 0xA;	
//	printf("\n\rtestvalue������Ϊ:0x%x,���ַΪ:0x%x\r\n",testvalue,&testvalue);
//	testvalue_double = 3.4356;
//	printf("testvalue_double������Ϊ:%f,���ַΪ:0x%x\r\n",testvalue_double,&testvalue_double);

	*p_iner = (uint8_t)0x79;
	printf("iner_sram:0x%x\n\r",*p_iner);
	*p_8 =  (uint8_t)0x23;
	printf("external_sram:0x%x\n\r", *p_8);
	*p_16 = (uint16_t)0x125F;
	printf("external_sram:0x%x\n\r", *p_16);
	*p_32 = (uint32_t)0x12345678;
	printf("external_sram:0x%x\n\r", *p_32);
	
	printf("���ڼ��SRAM����8λ��16λ�ķ�ʽ��дsram...");

	/*��8λ��ʽ��д���ݣ���У��*/
	/* ��SRAM����ȫ������Ϊ0 ��IS62WV51216_SIZE����8λΪ��λ�� */
	for (counter = 0x00; counter < IS62WV51216_SIZE; counter++)
	{
		*(__IO uint8_t*) (SRAM_BASE_ADDR + counter) = (uint8_t)0x0;
	}

	/* ������SRAMд������  8λ */
	for (counter = 0; counter < IS62WV51216_SIZE; counter++)
	{
		*(__IO uint8_t*) (SRAM_BASE_ADDR + counter) = (uint8_t)(ubWritedata_8b + counter);
	}

	/* ��ȡ SRAM ���ݲ����*/
	for(counter = 0; counter<IS62WV51216_SIZE;counter++ )
	{
		ubReaddata_8b = *(__IO uint8_t*)(SRAM_BASE_ADDR + counter);  //�Ӹõ�ַ��������

		if(ubReaddata_8b != (uint8_t)(ubWritedata_8b + counter))      //������ݣ�������ȣ���������,���ؼ��ʧ�ܽ����
		{
		  printf("8λ���ݶ�д����");
		  return 0;
		}
	}
	
	/*��16λ��ʽ��д���ݣ������*/
	/* ��SRAM����ȫ������Ϊ0 */
	for (counter = 0x00; counter < IS62WV51216_SIZE/2; counter++)
	{
		*(__IO uint16_t*) (SRAM_BASE_ADDR + 2*counter) = (uint16_t)0x00;
	}

	/* ������SRAMд������  16λ */
	for (counter = 0; counter < IS62WV51216_SIZE/2; counter++)
	{
		*(__IO uint16_t*) (SRAM_BASE_ADDR + 2*counter) = (uint16_t)(uhWritedata_16b + counter);
	}

	/* ��ȡ SRAM ���ݲ����*/
	for(counter = 0; counter<IS62WV51216_SIZE/2;counter++ )
	{
		uhReaddata_16b = *(__IO uint16_t*)(SRAM_BASE_ADDR + 2*counter);  //�Ӹõ�ַ��������

		if(uhReaddata_16b != (uint16_t)(uhWritedata_16b + counter))      //������ݣ�������ȣ���������,���ؼ��ʧ�ܽ����
		{
		  printf("16λ���ݶ�д����");
		  return 0;
		}
	}
  
	printf("SRAM��д����������"); 
	
	/*���������return 1 */
	return 1;
}

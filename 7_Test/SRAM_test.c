#include "SRAM_test.h"

//volatile uint8_t testvalue __attribute((at(0x6C000080)));
//volatile double testvalue_double __attribute((at(0x6C000100)));

/**
  * @brief  测试SRAM 
  * @param  None
  * @retval 正常返回1，异常返回0
  */
uint8_t sram_read_write_test(void)
{
    volatile uint8_t  * p_iner  = (uint8_t  *)INER_SRAM_ADDR;
	volatile uint8_t  * p_8		= (uint8_t  *)SRAM_BASE_ADDR;
	volatile uint16_t * p_16	= (uint16_t *)(SRAM_BASE_ADDR+10);
	volatile uint32_t * p_32	= (uint32_t *)(SRAM_BASE_ADDR+20);
	
	/*写入数据计数器*/
	uint32_t counter=0;

	/* 8位的数据 */
	uint8_t ubWritedata_8b = 0, ubReaddata_8b = 0;  

	/* 16位的数据 */
	uint16_t uhWritedata_16b = 0, uhReaddata_16b = 0; 
	
//	testvalue = 0xA;	
//	printf("\n\rtestvalue的内容为:0x%x,其地址为:0x%x\r\n",testvalue,&testvalue);
//	testvalue_double = 3.4356;
//	printf("testvalue_double的内容为:%f,其地址为:0x%x\r\n",testvalue_double,&testvalue_double);

	*p_iner = (uint8_t)0x79;
	printf("iner_sram:0x%x\n\r",*p_iner);
	*p_8 =  (uint8_t)0x23;
	printf("external_sram:0x%x\n\r", *p_8);
	*p_16 = (uint16_t)0x125F;
	printf("external_sram:0x%x\n\r", *p_16);
	*p_32 = (uint32_t)0x12345678;
	printf("external_sram:0x%x\n\r", *p_32);
	
	printf("正在检测SRAM，以8位、16位的方式读写sram...");

	/*按8位格式读写数据，并校验*/
	/* 把SRAM数据全部重置为0 ，IS62WV51216_SIZE是以8位为单位的 */
	for (counter = 0x00; counter < IS62WV51216_SIZE; counter++)
	{
		*(__IO uint8_t*) (SRAM_BASE_ADDR + counter) = (uint8_t)0x0;
	}

	/* 向整个SRAM写入数据  8位 */
	for (counter = 0; counter < IS62WV51216_SIZE; counter++)
	{
		*(__IO uint8_t*) (SRAM_BASE_ADDR + counter) = (uint8_t)(ubWritedata_8b + counter);
	}

	/* 读取 SRAM 数据并检测*/
	for(counter = 0; counter<IS62WV51216_SIZE;counter++ )
	{
		ubReaddata_8b = *(__IO uint8_t*)(SRAM_BASE_ADDR + counter);  //从该地址读出数据

		if(ubReaddata_8b != (uint8_t)(ubWritedata_8b + counter))      //检测数据，若不相等，跳出函数,返回检测失败结果。
		{
		  printf("8位数据读写错误！");
		  return 0;
		}
	}
	
	/*按16位格式读写数据，并检测*/
	/* 把SRAM数据全部重置为0 */
	for (counter = 0x00; counter < IS62WV51216_SIZE/2; counter++)
	{
		*(__IO uint16_t*) (SRAM_BASE_ADDR + 2*counter) = (uint16_t)0x00;
	}

	/* 向整个SRAM写入数据  16位 */
	for (counter = 0; counter < IS62WV51216_SIZE/2; counter++)
	{
		*(__IO uint16_t*) (SRAM_BASE_ADDR + 2*counter) = (uint16_t)(uhWritedata_16b + counter);
	}

	/* 读取 SRAM 数据并检测*/
	for(counter = 0; counter<IS62WV51216_SIZE/2;counter++ )
	{
		uhReaddata_16b = *(__IO uint16_t*)(SRAM_BASE_ADDR + 2*counter);  //从该地址读出数据

		if(uhReaddata_16b != (uint16_t)(uhWritedata_16b + counter))      //检测数据，若不相等，跳出函数,返回检测失败结果。
		{
		  printf("16位数据读写错误！");
		  return 0;
		}
	}
  
	printf("SRAM读写测试正常！"); 
	
	/*检测正常，return 1 */
	return 1;
}

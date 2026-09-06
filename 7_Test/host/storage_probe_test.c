/* Host fault injection for production read-only boot probes. The runner
 * extracts the probe functions verbatim into storage_probes.inc. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef enum {RESET, SET} FlagStatus;
typedef struct {uint32_t SR1, SR2;} I2c;
static I2c eeprom_i2c;
#define EEPROM_I2C (&eeprom_i2c)
#define EEPROM_TIME_OUT 16U
#define EEPROM_I2C_ADDR 0xa1U
#define I2C_SR1_AF 1U
#define I2C_SR1_BERR 2U
#define I2C_SR1_ARLO 4U
#define I2C_SR1_OVR 8U
#define I2C_FLAG_BUSY 10U
#define I2C_FLAG_SB 11U
#define I2C_FLAG_ADDR 12U
#define I2C_FLAG_BTF 13U
#define I2C_FLAG_RXNE 14U
#define I2C_Direction_Transmitter 0U
#define I2C_Direction_Receiver 1U
#define I2C_NACKPosition_Current 0U
#define ENABLE 1U
#define DISABLE 0U
static uint8_t nvram[256], address, receiving, ack, pointer_sent;
static unsigned read_calls, pointer_calls, flag_calls, nack, unstable, fault_flag;
static uint32_t primask;
static void I2C_AcknowledgeConfig(I2c *bus, uint8_t value) {(void)bus; ack=value;}
static void I2C_NACKPositionConfig(I2c *bus, uint8_t value) {(void)bus;(void)value;}
static FlagStatus I2C_GetFlagStatus(I2c *bus, uint32_t flag)
{
    (void)bus;
    assert(++flag_calls < 10000U); /* No unbounded waits on a missing device. */
    if(flag == fault_flag) return flag == I2C_FLAG_BUSY ? SET : RESET;
    return flag == I2C_FLAG_BUSY ? RESET : SET;
}
static void I2C_GenerateSTART(I2c *bus, uint8_t value) {(void)bus;(void)value;}
static void I2C_Send7bitAddress(I2c *bus, uint8_t addr, uint8_t direction)
{
    assert(addr == 0xa0U);
    receiving = direction;
    if(!direction) pointer_sent=0U;
    if(nack) bus->SR1 |= I2C_SR1_AF;
}
static void I2C_SendData(I2c *bus, uint8_t data)
{
    (void)bus;
    assert(!receiving && !pointer_sent); /* Only a word-address byte, never data. */
    pointer_sent=1U;
    pointer_calls++;
    address=data;
}
static uint8_t I2C_ReceiveData(I2c *bus)
{
    (void)bus;
    assert(!ack && receiving);
    return (uint8_t)(nvram[address] ^ ((unstable && (read_calls++ & 1U)) ? 1U : 0U));
}
static void I2C_GenerateSTOP(I2c *bus, uint8_t value) {(void)bus;(void)value;}
static void I2C_DeInit(I2c *bus) {memset(bus,0,sizeof(*bus));}
static void EEPROM_I2C_Init(void) {ack=1U;}
static uint32_t __get_PRIMASK(void) {return primask;}
static void __disable_irq(void) {primask=1U;}
static void __set_PRIMASK(uint32_t value) {primask=value;}

#define FLASH_ID 0xef4018UL
#define SPI_FLASH_PARAM_ADDR 0x5ff000UL
#define SPI_FLASH_LAYOUT_TOTAL_SIZE 0x1000000UL
#define W25X_ReadStatusReg 5U
#define WIP_Flag 1U
static uint8_t flash_io_error;
static uint32_t fake_id;
static unsigned flash_busy, flash_unstable, flash_reads, flash_delay, flash_fault;
static unsigned flash_selected;
static void flash_select(unsigned selected) {flash_selected=selected;}
#define FLASH_SPI_CS_LOW flash_select(1U)
#define FLASH_SPI_CS_HIGH flash_select(0U)
static void FLASH_Wakeup(void) {}
static void Delay_us(unsigned n) {(void)n;}
static void Delay_ms(unsigned n) {flash_delay+=n; assert(flash_delay<=500U);}
static uint32_t FLASH_Read_FlashID(void)
{
    if(flash_fault) flash_io_error=1U;
    return fake_id;
}
static void FLASH_Send_Byte(uint8_t command)
{
    assert(flash_selected && command == W25X_ReadStatusReg);
}
static uint8_t FLASH_Receive_Byte(void) {return flash_busy ? WIP_Flag : 0U;}
static void FLASH_Read_Data(uint8_t *data, uint32_t addr, uint16_t size)
{
    assert(addr == 0U || addr == SPI_FLASH_PARAM_ADDR ||
           addr == SPI_FLASH_LAYOUT_TOTAL_SIZE-32U);
    memset(data,0xff,size);
    if(flash_unstable && (flash_reads & 1U)) data[0]=0;
    flash_reads++;
}

#include "storage_probes.inc"

static void eeprom_reset(void)
{
    memset(&eeprom_i2c,0,sizeof(eeprom_i2c));
    receiving=pointer_sent=0U;
    ack=1U;
    read_calls=pointer_calls=flag_calls=nack=unstable=fault_flag=0U;
    primask=0U;
}
static void flash_reset(void)
{
    fake_id=FLASH_ID;
    flash_io_error=0U;
    flash_busy=flash_unstable=flash_reads=flash_delay=flash_fault=flash_selected=0U;
}

int main(void)
{
    uint8_t before[256], data;
    unsigned i;
    uint32_t id;
    for(i=0;i<256;i++) nvram[i]=(uint8_t)(i*17U);
    memcpy(before,nvram,sizeof(before));
    eeprom_reset();
    assert(EEPROM_BootProbe() == 0U && pointer_calls == 512U);
    assert(ack && primask == 0U && memcmp(before,nvram,sizeof(before)) == 0);
    memset(nvram,0xff,sizeof(nvram));
    eeprom_reset();
    assert(EEPROM_BootProbe() == 0U); /* Blank EEPROM is healthy. */
    for(i=I2C_FLAG_BUSY; i<=I2C_FLAG_RXNE; i++) {
        eeprom_reset(); fault_flag=i;
        assert(EEPROM_BootProbe() != 0U);
        assert(ack && primask == 0U);
    }
    eeprom_reset(); nack=1U;
    assert(EEPROM_BootProbe() != 0U);
    eeprom_reset(); unstable=1U;
    assert(EEPROM_BootProbe() == 9U);
    eeprom_reset(); primask=1U;
    assert(EEPROM_Random_Read(0,&data) == 0U && primask == 1U);
    assert(EEPROM_Random_Read(0,0) != 0U);

    flash_reset();
    assert(FLASH_BootProbe(&id) == 0U && id == FLASH_ID && flash_reads == 6U);
    flash_reset(); fake_id=0xffffffU;
    assert(FLASH_BootProbe(&id) == 2U && flash_reads == 0U);
    flash_reset(); fake_id=0U;
    assert(FLASH_BootProbe(&id) == 2U);
    flash_reset(); flash_fault=1U;
    assert(FLASH_BootProbe(&id) == 1U);
    flash_reset(); flash_busy=1U;
    assert(FLASH_BootProbe(&id) == 3U && flash_delay == 500U && !flash_selected);
    flash_reset(); flash_unstable=1U;
    assert(FLASH_BootProbe(&id) == 4U);
    puts("PASS: EEPROM timeout/NACK/ACK recovery; Flash ID/busy/I/O/mismatch; no test writes");
    return 0;
}

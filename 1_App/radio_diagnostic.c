#include "hardware_tests.h"
#include "bsp_spi_nrf.h"
#include "bsp_Systick.h"
#include "multi_button_user.h"
#include <string.h>
#include <stdio.h>

/* Diagnostic transactions preserve CE during status reads. Legacy ReadReg lowers
 * CE and must not be used while waiting for a receive packet. All SPI waits bound. */
static uint8_t io_error;
static uint8_t transfer(uint8_t byte)
{
    uint32_t wait = 100000UL;
    while(SPI_I2S_GetFlagStatus(NRF_SPI, SPI_I2S_FLAG_TXE) == RESET)
        if(--wait == 0U) { io_error = 1U; return 0xffU; }
    SPI_I2S_SendData(NRF_SPI, byte);
    wait = 100000UL;
    while(SPI_I2S_GetFlagStatus(NRF_SPI, SPI_I2S_FLAG_RXNE) == RESET)
        if(--wait == 0U) { io_error = 1U; return 0xffU; }
    return (uint8_t)SPI_I2S_ReceiveData(NRF_SPI);
}

static void transaction(uint8_t command, uint8_t *data, uint8_t size, uint8_t write)
{
    uint8_t i;
    NRF_CSN_LOW();
    (void)transfer(command);
    for(i = 0U; i < size; i++) {
        uint8_t rx = transfer(write ? data[i] : 0xffU);
        if(!write) data[i] = rx;
    }
    NRF_CSN_HIGH();
}
static uint8_t reg_read(uint8_t reg)
{
    uint8_t value;
    transaction(reg, &value, 1U, 0U);
    return value;
}
static void reg_write(uint8_t reg, uint8_t value)
{
    transaction(NRF_WRITE_REG | reg, &value, 1U, 1U);
}

HwResult hardware_radio_test(uint8_t receive)
{
    static const uint8_t regs[] = {CONFIG, EN_AA, EN_RXADDR, SETUP_AW, SETUP_RETR,
        RF_CH, RF_SETUP, RX_PW_P0, 0x1cU}; /* DYNPD: fixed 32-byte diagnostic packets */
    uint8_t old[sizeof(regs)], tx[5], rx[5], packet[32], received[32];
    uint8_t address[5] = {0xd7U, 0x43U, 0x44U, 0x47U, 0x31U};
    uint8_t i, count = 0U, status, old_ce;
    uint16_t ms = 0U;
    HwResult result = HW_FAIL;
    io_error = 0U;
    old_ce = GPIO_ReadOutputDataBit(NRF_CE_GPIO_PORT, NRF_CE_PIN);
    NRF_CE_LOW();
    Delay_us(150U);
    status = reg_read(FIFO_STATUS);
    /* Do not discard normal traffic to make room for the diagnostic. */
    if(io_error || status == 0xffU) goto no_change;
    i = reg_read(SETUP_AW);
    if(io_error || i < 1U || i > 3U) goto no_change;
    if((status & 0x11U) != 0x11U) { result = HW_BLOCKED; goto no_change; }
    status = reg_read(STATUS);
    if(io_error || (status & 0x70U)) { result = HW_BLOCKED; goto no_change; }
    for(i = 0; i < sizeof(regs); i++) old[i] = reg_read(regs[i]);
    transaction(TX_ADDR, tx, 5U, 0U);
    transaction(RX_ADDR_P0, rx, 5U, 0U);
    if(io_error) goto no_change;
    reg_write(CONFIG, 0x0cU); /* Power down before reconfiguration. */
    reg_write(EN_AA, 1U); reg_write(EN_RXADDR, 1U); reg_write(SETUP_AW, 3U);
    reg_write(SETUP_RETR, 0x3fU); reg_write(RF_CH, 40U); reg_write(RF_SETUP, 0x06U);
    reg_write(RX_PW_P0, 32U); reg_write(0x1cU, 0U);
    transaction(NRF_WRITE_REG | TX_ADDR, address, 5U, 1U);
    transaction(NRF_WRITE_REG | RX_ADDR_P0, address, 5U, 1U);
    for(i = 0; i < 32U; i++) packet[i] = (uint8_t)(0x6dU ^ (i * 17U));
    reg_write(CONFIG, receive ? 0x0fU : 0x0eU);
    Delay_ms(2U);
    /* Check the settings that make an ACK meaningful; reject all-ones absent devices. */
    if(io_error || reg_read(CONFIG) != (receive ? 0x0fU : 0x0eU) ||
       reg_read(EN_AA) != 1U || reg_read(RF_CH) != 40U || reg_read(RF_SETUP) != 0x06U)
        goto cleanup;
    if(receive) NRF_CE_HIGH();
    else {
        transaction(WR_TX_PLOAD, packet, 32U, 1U);
        NRF_CE_HIGH(); Delay_us(20U); NRF_CE_LOW();
    }
    for(ms = 0; ms < 15000U; ms++) {
        status = reg_read(STATUS);
        if(io_error || status == 0xffU) goto cleanup;
        if(!read_button_back_gpio(0U)) { result = HW_CANCELLED; goto cleanup; }
        if(receive && (status & RX_DR)) {
            transaction(RD_RX_PLOAD, received, 32U, 0U);
            if(io_error || memcmp(packet, received, 32U)) goto cleanup;
            reg_write(STATUS, RX_DR);
            if(++count == 8U) { result = HW_PASS; break; }
        } else if(!receive && (status & TX_DS)) {
            reg_write(STATUS, TX_DS);
            if(++count == 8U) { result = HW_PASS; break; }
            Delay_ms(20U);
            transaction(WR_TX_PLOAD, packet, 32U, 1U);
            NRF_CE_HIGH(); Delay_us(20U); NRF_CE_LOW();
        }
        if(!receive && ((status & MAX_RT) || ms >= 2000U)) goto cleanup;
        Delay_ms(1U);
    }
cleanup:
    NRF_CE_LOW();
    Delay_ms(2U); /* Allow the last RX auto-ACK to finish before power down. */
    reg_write(CONFIG, 0x0cU);
    transaction(FLUSH_TX, packet, 0U, 1U);
    transaction(FLUSH_RX, packet, 0U, 1U);
    reg_write(STATUS, 0x70U);
    transaction(NRF_WRITE_REG | TX_ADDR, tx, 5U, 1U);
    transaction(NRF_WRITE_REG | RX_ADDR_P0, rx, 5U, 1U);
    for(i = 1U; i < sizeof(regs); i++) reg_write(regs[i], old[i]);
    reg_write(CONFIG, old[0]);
    Delay_ms(2U);
    for(i = 0; i < sizeof(regs); i++) if(reg_read(regs[i]) != old[i]) result = HW_FAIL;
    transaction(TX_ADDR, packet, 5U, 0U);
    transaction(RX_ADDR_P0, packet + 5, 5U, 0U);
    if(memcmp(packet, tx, 5U) || memcmp(packet + 5, rx, 5U) || io_error) result = HW_FAIL;
no_change:
    if(old_ce) NRF_CE_HIGH();
    printf("[DIAG] radio %s: packets=%u/8 ms=%u result=%s\r\n",
           receive ? "RX" : "TX+ACK", count, ms, hardware_result_name(result));
    return result;
}

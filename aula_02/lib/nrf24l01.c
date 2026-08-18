#include "nrf24l01.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>

uint8_t nrf24_reg(const struct spi_dt_spec *spi_dev, uint8_t reg, uint8_t data) 
{
    uint8_t tx_data[2] = {reg, data};
    uint8_t rx_data[2] = {0};

    struct spi_buf tx_buf = { .buf = tx_data, .len = 2 };
    struct spi_buf rx_buf = { .buf = rx_data, .len = 2 };
    struct spi_buf_set tx_set = { .buffers = &tx_buf, .count = 1 };
    struct spi_buf_set rx_set = { .buffers = &rx_buf, .count = 1 };

   
    spi_transceive_dt(spi_dev, &tx_set, &rx_set);

    return rx_data[1]; 
}

void nrf24_write_register(const struct spi_dt_spec *spi_dev, uint8_t reg, uint8_t data) 
{
    nrf24_reg(spi_dev, reg | 0x20, data);
}

uint8_t nrf24_read_register(const struct spi_dt_spec *spi_dev, uint8_t reg) 
{
    return nrf24_reg(spi_dev, reg, 0xff);
}

void nrf24_init(const struct spi_dt_spec *spi_dev) 
{
    nrf24_write_register(spi_dev, CONFIG, 0x0F); // CONFIG: PWR_UP=1, PRIM_RX=1
    nrf24_write_register(spi_dev, EN_AA, 0xFF); 
    nrf24_write_register(spi_dev, EN_RXADDR, 0x00); // EN_RXADDR: Enable data pipes 0 and 1
    nrf24_write_register(spi_dev, SETUP_RETR, 0x03); // SETUP_RETR: Auto retransmit delay = 1000us, count = 3
    nrf24_write_register(spi_dev, RF_CH, 0x02); // RF_CH: Set RF channel to 2
    nrf24_write_register(spi_dev, RF_SETUP, 0x17); // RF_SETUP: Set data rate to 1Mbps and power to max se der merda é culpa do 0x17
    nrf24_write_register(spi_dev, STATUS, 0x70); // STATUS: Clear any pending interrupts
    nrf24_write_register(spi_dev, RX_ADDR_P0, 0xE7); // RX_ADDR_P0: Set address for data pipe 0
    nrf24_write_register(spi_dev, FLUSH_RX, 0x00); // Flush RX FIFO
    nrf24_write_register(spi_dev, FLUSH_TX, 0x00); // Flush TX FIFO
}


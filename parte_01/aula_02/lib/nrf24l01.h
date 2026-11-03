#ifndef NRF24L01_H
#define NRF24L01_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>

extern struct k_sem radio_rx_sem;

uint8_t nrf24_init(const struct spi_dt_spec *spi_dev);

void nrf24_radio_enable();
void nrf24_radio_disable();

void    nrf24_send_command(const struct spi_dt_spec *spi_dev, uint8_t command);
void    nrf24_write_register(const struct spi_dt_spec *spi_dev, uint8_t reg, uint8_t data);
uint8_t nrf24_read_register(const struct spi_dt_spec *spi_dev, uint8_t reg);

void nrf24_write_buffer(const struct spi_dt_spec *spi_dev, uint8_t reg, const uint8_t *data, size_t lenght);
void nrf24_read_buffer(const struct spi_dt_spec *spi_dev, uint8_t command, uint8_t *data, size_t lenght);

//void nrf24_trigger_ack(const struct spi_dt_spec *spi_dev, uint8_t *data, size_t lenght);

#endif /* NRF24L01_H */
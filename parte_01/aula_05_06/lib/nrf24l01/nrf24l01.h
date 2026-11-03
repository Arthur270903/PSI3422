#ifndef NRF24L01_H
#define NRF24L01_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>

#define NRF24_REG_CONFIG      0x00
#define NRF24_REG_STATUS      0x07
#define NRF24_REG_RX_ADDR_P0  0x0a
#define NRF24_REG_TX_ADDR     0x10

#define NRF24_CONFIG_PRIM_TX  0x0e /* PWR_UP | CRCO | EN_CRC, PRIM_RX=0 */
#define NRF24_CONFIG_PRIM_RX  0x0f /* PWR_UP | CRCO | EN_CRC, PRIM_RX=1 */

#define NRF24_STATUS_RX_DR    0x40
#define NRF24_STATUS_TX_DS    0x20
#define NRF24_STATUS_MAX_RT   0x10
#define NRF24_STATUS_CLEAR_ALL (NRF24_STATUS_RX_DR | NRF24_STATUS_TX_DS | NRF24_STATUS_MAX_RT)

#define NRF24_CMD_FLUSH_TX     0xe1
#define NRF24_CMD_FLUSH_RX     0xe2
#define NRF24_CMD_W_TX_PAYLOAD 0xa0
#define NRF24_CMD_R_RX_PAYLOAD 0x61

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
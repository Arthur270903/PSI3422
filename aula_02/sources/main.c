#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>

#include "nrf24l01.h"

#define SPI_DEVICE_NODE DT_NODELABEL(spi_dev_a)

#define TRANSMITTER (0)

static const struct spi_dt_spec spi_dev = SPI_DT_SPEC_GET(
    SPI_DEVICE_NODE, 
    SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_TRANSFER_MSB
);

int main()
{   
    if (!spi_is_ready_dt(&spi_dev)) 
    {
        printk("Erro: Dispositivo SPI\n");
        return 0;
    }

    uint8_t address[5] = { 0xc2, 0xc2, 0xc2, 0xc2, 0xc2 };

    if(nrf24_init(&spi_dev))
    {
        printk("Erro: Dispositivo NRF24\n");
        return 0;
    }

#if (TRANSMITTER)
    printk("--- transmitter ---\n");

    nrf24_radio_disable();
    nrf24_write_register(&spi_dev, 0x00, 0x0e); // CONFIG: PRIM_TX (modo tx)

    nrf24_write_buffer(&spi_dev, 0x10, address, 5); // TX_ADDR
    nrf24_write_buffer(&spi_dev, 0xa0, address, 5); // RX_ADDR_P0

    while (1) 
    {
        uint8_t message[] = "Hello world";;

        nrf24_write_buffer(&spi_dev, 0xa0, message, sizeof(message));
        nrf24_radio_enable();
        k_busy_wait(20); // o mínimo é de 10us
        nrf24_radio_disable();

        k_msleep(2000);
    }

#else
    printk("--- receiver ---\n");

    nrf24_radio_disable();
    nrf24_write_register(&spi_dev, 0x00, 0x0f); // CONFIG: PRIM_RX (Modo Rx)

    nrf24_write_buffer(&spi_dev, 0x0a, address, 5); // RX_ADDR_P0
    nrf24_radio_enable();

    //uint8_t data = "ACK OK";
    //nrf24_write_buffer(&spi_dev, 0xa8, data, sizeof(data));

    while (1)
    {
        if (k_sem_take(&radio_rx_sem, K_FOREVER) == 0)
        {
            uint8_t payload[32] = {0};
            nrf24_read_buffer(&spi_dev, 0x61, payload, 32);

            printk("Pacote Recebido: %s\n", payload);
        }
    }
#endif

    return 0;
}
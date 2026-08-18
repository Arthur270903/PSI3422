#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>
#include "nrf24l01.h"

#define SPI_DEVICE_NODE DT_NODELABEL(spi_dev_a)

static const struct spi_dt_spec spi_dev = SPI_DT_SPEC_GET(
    SPI_DEVICE_NODE, 
    SPI_WORD_SET(8) | SPI_TRANSFER_MSB
);




int main(void)
{   
    if (!spi_is_ready_dt(&spi_dev)) 
    {
        printk("Erro: Dispositivo SPI ou pino de CS não estão prontos!\n");
        return 0;
    }
    nrf24_init(&spi_dev);
    while (1) 
    {
        
       uint8_t payload = nrf24_read_register(&spi_dev, R_RX_PAYLOAD);
        printk("Payload: 0x%02X\n", payload);
        k_msleep(1000);
    }

    return 0;
}
#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>

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

    uint8_t tx_data[] = {0xAA, 0xBB};
    uint8_t rx_data[2] = {0};

    struct spi_buf tx_buf = {.buf = tx_data, .len = sizeof(tx_data)};
    struct spi_buf rx_buf = {.buf = rx_data, .len = sizeof(rx_data)};

    struct spi_buf_set tx_bufs = {.buffers = &tx_buf, .count = 1};
    struct spi_buf_set rx_bufs = {.buffers = &rx_buf, .count = 1};

    printk("Iniciando transações SPI com o nRF24...\n");

    while (1) 
    {
        /* Envia e recebe usando a API '*_dt' (Device Tree) */
        int err = spi_transceive_dt(&spi_dev, &tx_bufs, &rx_bufs);
        
        if (err == 0)
            printk("RX: 0x%02X 0x%02X\n", rx_data[0], rx_data[1]);
        else
            printk("Falha na transacao SPI. Erro: %d\n", err);

        k_msleep(1000);
    }

    return 0;
}
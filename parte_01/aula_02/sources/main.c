#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/uart.h>

#include "nrf24l01.h"

#define SPI_DEVICE_NODE DT_NODELABEL(spi_dev_a)

#define TRANSMITTER (0)

static struct spi_dt_spec spi_dev = SPI_DT_SPEC_GET(
    SPI_DEVICE_NODE, 
    SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_TRANSFER_MSB
);

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct device *console_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

int main()
{   
    if (!spi_is_ready_dt(&spi_dev)) 
    {
        printk("Erro: Dispositivo SPI\n");
        return 0;
    }

    uint8_t address[5] = { 0xc2, 0xc2, 0xc2, 0xc2, 0xc2 };

    uint8_t test_data = nrf24_read_register(&spi_dev, 0x00);
    printk("CONFIG: 0x%02x\n", test_data);

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
    nrf24_write_buffer(&spi_dev, 0x0a, address, 5); // RX_ADDR_P0

    while (1) 
    {
        uint8_t c;

        if (uart_poll_in(console_dev, &c) == 0) {
            
            // Só faz algo se o usuário digitar '1' ou '0'
            if (c == '1' || c == '0') {
                uint8_t payload = (c == '1') ? 1 : 0;
                
                printk("Enviando comando: %c\n", c);

                nrf24_send_command(&spi_dev, 0xe1);

                nrf24_write_buffer(&spi_dev, 0xa0, &payload, 1); 

                nrf24_radio_enable();
                k_busy_wait(15); 
                nrf24_radio_disable();
                
                k_msleep(5);
                nrf24_write_register(&spi_dev, 0x07, 0x70); 
            }
        }
        
        k_msleep(10);
    }

#else
    printk("--- receiver ---\n");

    if (gpio_is_ready_dt(&led))
        gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);

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
            uint8_t status = nrf24_read_register(&spi_dev, 0x07);

            if (status & 0x10)
            {
                printk("Erro: Sem resposta.\n");
                nrf24_send_command(&spi_dev, 0xe1);
            }
            if (status & 0x20)
                printk("Enviado com sucesso.\n");
            if (status & 0x40)
            {
                uint8_t payload = 0;
                nrf24_read_buffer(&spi_dev, 0x61, &payload, 1);

                printk("Pacote Recebido: %d\n", payload);

                gpio_pin_set_dt(&led, payload);
            }

            nrf24_write_register(&spi_dev, 0x07, status & 0x70);
            nrf24_send_command(&spi_dev, 0xe2);
        }
    }
#endif

    return 0;
}

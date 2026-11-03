#include "radio_link.h"

#include "nrf24l01.h"

#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_radio_link, LOG_LEVEL_INF);

#define SPI_DEVICE_NODE DT_NODELABEL(spi_dev_a)

static struct spi_dt_spec spi_dev = SPI_DT_SPEC_GET(SPI_DEVICE_NODE, SPI_OP_MODE_CONTROLLER | SPI_WORD_SET(8) | SPI_TRANSFER_MSB);
static const struct device *console_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

// ---

void nrf24l01_entry_point(void *, void *, void *)
{
    if (!spi_is_ready_dt(&spi_dev)) 
    {
        LOG_ERR("Dispositivo SPI nao esta pronto");
        return;
    }

    uint8_t address[5] = { 0xc2, 0xc2, 0xc2, 0xc2, 0xc2 };

    uint8_t test_data = nrf24_read_register(&spi_dev, NRF24_REG_CONFIG);
    LOG_INF("CONFIG: 0x%02x", test_data);

    if(nrf24_init(&spi_dev))
    {
        LOG_ERR("Falha ao inicializar o NRF24");
        return;
    }

#if defined(CONFIG_APP_ROLE_TRANSMITTER)
    LOG_INF("--- transmitter ---");

    nrf24_radio_disable();
    nrf24_write_register(&spi_dev, 0x00, 0x0e); // CONFIG: PRIM_TX (modo tx)

    test_data = nrf24_read_register(&spi_dev, NRF24_REG_CONFIG);
    LOG_INF("-CONFIG: 0x%02x", test_data);
    nrf24_write_buffer(&spi_dev, 0x10, address, 5); // TX_ADDR
    nrf24_write_buffer(&spi_dev, 0x0a, address, 5); // RX_ADDR_P0

    while (1) 
    {
        uint8_t c;

        if (uart_poll_in(console_dev, &c) == 0) 
        {
            if (c == COMMAND_RUN || c == COMMAND_STOP || c == COMMAND_PRINT || c == COMMAND_CLEAN) 
            {    
                LOG_INF("Enviando comando: %c", c);

                nrf24_send_command(&spi_dev, NRF24_CMD_FLUSH_TX);
                nrf24_write_buffer(&spi_dev, NRF24_CMD_W_TX_PAYLOAD, &c, 1); 

                nrf24_radio_enable();
                k_busy_wait(15); 
                nrf24_radio_disable();
                
                k_msleep(5);
                    LOG_INF("CONFIG: 0x%02x", test_data);
                nrf24_write_register(&spi_dev, NRF24_REG_STATUS, NRF24_STATUS_CLEAR_ALL); 
            }
        }
        
        k_msleep(10);
    }

#else
    LOG_INF("--- receiver ---");

    nrf24_radio_disable();
    nrf24_write_register(&spi_dev, NRF24_REG_CONFIG, NRF24_CONFIG_PRIM_RX); // CONFIG: PRIM_RX (Modo Rx)

    nrf24_write_buffer(&spi_dev, NRF24_REG_RX_ADDR_P0, address, 5); // RX_ADDR_P0
    nrf24_radio_enable();

    while (1)
    {
        if (k_sem_take(&radio_rx_sem, K_FOREVER) == 0)
        {
            uint8_t status = nrf24_read_register(&spi_dev, NRF24_REG_STATUS);

            if (status & NRF24_STATUS_MAX_RT)
            {
                LOG_WRN("Sem resposta (MAX_RT)");
                nrf24_send_command(&spi_dev, NRF24_CMD_FLUSH_TX);
            }
            if (status & NRF24_STATUS_TX_DS)
                LOG_INF("Enviado com sucesso");

            if (status & NRF24_STATUS_RX_DR)
            {
                uint8_t payload = 0;
                nrf24_read_buffer(&spi_dev, NRF24_CMD_R_RX_PAYLOAD, &payload, 1);

                LOG_INF("Pacote recebido: %c", payload);

                command_t command = (command_t)payload;
                k_msgq_put(&command_msgq, &command, K_NO_WAIT);
            }
            
            nrf24_write_register(&spi_dev, NRF24_REG_STATUS, status & NRF24_STATUS_CLEAR_ALL);
            nrf24_send_command(&spi_dev, NRF24_CMD_FLUSH_RX);
        }
    }
#endif /* defined(CONFIG_APP_ROLE_TRANSMITTER) */
}

// ---
K_THREAD_DEFINE(nrf24l01_tid, 512,
                nrf24l01_entry_point, NULL, NULL, NULL,
                0, 0, 0);
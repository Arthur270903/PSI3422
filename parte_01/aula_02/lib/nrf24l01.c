#include "nrf24l01.h"

K_SEM_DEFINE(radio_rx_sem, 0, 1)

static const struct gpio_dt_spec ce_pin  = GPIO_DT_SPEC_GET(DT_NODELABEL(spi_dev_a), ce_gpios);
static const struct gpio_dt_spec irq_pin = GPIO_DT_SPEC_GET(DT_NODELABEL(spi_dev_a), irq_gpios);

static struct gpio_callback irq_callback_data;

static const struct spi_dt_spec *radio_spi;

void nrf24_radio_enable(void) 
{
    gpio_pin_set_dt(&ce_pin, 1);
}

void nrf24_radio_disable(void) 
{
    gpio_pin_set_dt(&ce_pin, 0);
}

void nrf24_send_command(const struct spi_dt_spec *spi_dev, uint8_t command) 
{
    nrf24_write_buffer(spi_dev, command, NULL, 0);
}

void nrf24_write_register(const struct spi_dt_spec *spi_dev, uint8_t reg, uint8_t data) 
{
    nrf24_write_buffer(spi_dev, reg, &data, 1);
}

uint8_t nrf24_read_register(const struct spi_dt_spec *spi_dev, uint8_t reg) 
{
    uint8_t data = 0;
    nrf24_read_buffer(spi_dev, reg, &data, 1);

    return data;
}

void nrf24_write_buffer(const struct spi_dt_spec *spi_dev, uint8_t reg, const uint8_t *data, size_t lenght)
{
    uint8_t command = (reg < 0x20) ? (reg | 0x20) : reg;

    struct spi_buf tx_buffers[] = { {.buf = &command, .len = 1 }, { .buf = (void *)data, .len = lenght } };
    struct spi_buf_set tx_buffer_set = { .buffers = tx_buffers, .count = 2 };

    int ret = spi_write_dt(spi_dev, &tx_buffer_set);
    if (ret < 0)
        printk("SPI TX Error: %d\n", ret);
}

void nrf24_read_buffer(const struct spi_dt_spec *spi_dev, uint8_t command, uint8_t *data, size_t lenght)
{
    uint8_t dummy_tx[32] = {0};
    uint8_t dummy_rx;
    
    struct spi_buf tx_buffers[] = { 
        { .buf = &command, .len = 1 }, 
        { .buf = dummy_tx, .len = lenght } 
    };
    struct spi_buf_set tx_buffer_set = { .buffers = tx_buffers, .count = 2 };

    struct spi_buf rx_buffers[] = { 
        { .buf = &dummy_rx, .len = 1 }, 
        { .buf = data, .len = lenght } 
    };
    struct spi_buf_set rx_buffer_set = { .buffers = rx_buffers, .count = 2 };

    int ret =  spi_transceive_dt(spi_dev, &tx_buffer_set, &rx_buffer_set);
    if (ret < 0)
        printk("SPI RX Error: %d\n", ret);
}

void nrf24_irq_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) 
{

    k_sem_give(&radio_rx_sem);
}

uint8_t nrf24_init(const struct spi_dt_spec *spi_dev) 
{
    k_msleep(100); // power on reset

    if (!gpio_is_ready_dt(&ce_pin))
    {
        printk("Erro: Pino CE não está pronto");
        return -ENODEV;
    }

    if (!gpio_is_ready_dt(&irq_pin))
    {
        printk("Erro: Pino IRQ não está pronto");
        return -ENODEV;
    }

    radio_spi = spi_dev;

    gpio_pin_configure_dt(&ce_pin, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&irq_pin, GPIO_INPUT);

    gpio_pin_interrupt_configure_dt(&irq_pin, GPIO_INT_EDGE_TO_ACTIVE);

    gpio_init_callback(&irq_callback_data, nrf24_irq_handler, 1 << irq_pin.pin);
    gpio_add_callback(irq_pin.port, &irq_callback_data);

    nrf24_write_register(spi_dev, 0x00, 0x0f); // CONFIG: PWR_UP, PRIM_RX
    k_msleep(2); // wait

    nrf24_write_register(spi_dev, 0x01, 0x3f); // EN_AA: Auto ACK
    nrf24_write_register(spi_dev, 0x02, 0x01); // EN_RXADDR: Enable Pipe 0
    nrf24_write_register(spi_dev, 0x04, 0x03); // SETUP_RETR:
    nrf24_write_register(spi_dev, 0x05, 0x10); // RF_CH: Canal 0x10
    nrf24_write_register(spi_dev, 0x06, 0x07); // RF_SETUP: 1Mbps, Max Power

    nrf24_write_register(spi_dev, 0x1d, 0x06); // FEATURE: EN_DPL, EN_ACK_PAY TODO:
    nrf24_write_register(spi_dev, 0x1c, 0x01); // DYNPD: DPL_P1

    nrf24_write_register(spi_dev, 0x07, 0x70); // STATUS: CLEAN FLAGS
    nrf24_send_command(spi_dev, 0xE1); // FLUSH_TX
    nrf24_send_command(spi_dev, 0xE2); // FLUSH_RX

    return 0;
}


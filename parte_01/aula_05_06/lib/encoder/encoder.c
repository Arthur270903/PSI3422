#include "encoder.h"

#include <zephyr/sys/atomic.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app_encoder, LOG_LEVEL_INF);

static const struct gpio_dt_spec encoder_left  = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), irsensor0_gpios);
static const struct gpio_dt_spec encoder_right = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), irsensor1_gpios);

static struct gpio_callback encoder_cb_data;

static atomic_t left_pulses = ATOMIC_INIT(0);
static atomic_t right_pulses = ATOMIC_INIT(0);

static atomic_t target_pulses = ATOMIC_INIT(0);
static struct k_sem *target_reached_sem = NULL;

/* Debounce curto: filtra o "chatter" eletrico do comparador LM393 quando o
 * sensor fica numa distancia ambigua (ex.: bloqueado por um objeto), sem
 * descartar pulsos reais do disco (que costumam vir vários ms distantes
 * um do outro, mesmo em rotacao alta). Ajuste se necessario. */
#define DEBOUNCE_MS (2)

static void encoder_irq_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    static int64_t last_left_pulse_time  = 0;
    static int64_t last_right_pulse_time = 0;

    int64_t now_time = k_uptime_get();

    if ((pins & (1 << encoder_left.pin)) && (now_time - last_left_pulse_time) >= DEBOUNCE_MS)
    {
        atomic_inc(&left_pulses);
        last_left_pulse_time = now_time;
    }

    if ((pins & ( 1 << encoder_right.pin)) && (now_time - last_right_pulse_time) >= DEBOUNCE_MS)
    {
        atomic_inc(&right_pulses);
        last_right_pulse_time = now_time;
    }

    uint32_t target = (uint32_t)atomic_get(&target_pulses);
    
    if (target > 0 && target_reached_sem != NULL) 
    {
        uint32_t media = (atomic_get(&left_pulses) + atomic_get(&right_pulses)) / 2;
        LOG_INF("MEDIA: %d", media);
        if (media >= target) 
        {
            atomic_set(&target_pulses, 0); // Desativa o gatilho
            k_sem_give(target_reached_sem);
        }
    }
}

void encoders_init()
{
    if (!gpio_is_ready_dt(&encoder_left) || !gpio_is_ready_dt(&encoder_right))
    {
        printk("Erro: sensores ou pinos do motor nao estao prontos\n");
        return;
    }

    gpio_pin_configure_dt(&encoder_left, GPIO_INPUT);
    gpio_pin_configure_dt(&encoder_right, GPIO_INPUT);

    gpio_pin_interrupt_configure_dt(&encoder_left, GPIO_INT_EDGE_BOTH);
    gpio_pin_interrupt_configure_dt(&encoder_right, GPIO_INT_EDGE_BOTH);

    gpio_init_callback(&encoder_cb_data, encoder_irq_handler,
                        (1 << encoder_left.pin) | (1 << encoder_right.pin));

    gpio_add_callback(encoder_left.port, &encoder_cb_data);
}

void encoder_reset() 
{
    atomic_set(&left_pulses, 0);
    atomic_set(&right_pulses, 0);
}

uint32_t encoder_get_avg_pulses() 
{
    return (atomic_get(&left_pulses) + atomic_get(&right_pulses)) / 2;
}

uint32_t encoder_get_left_pulses()
{ 
    return atomic_get(&left_pulses); 
}
uint32_t encoder_get_right_pulses() 
{ 
    return atomic_get(&right_pulses); 
}

void encoder_set_target_notify(uint32_t pulses, struct k_sem *sem) 
{
    target_reached_sem = sem;
    atomic_set(&target_pulses, pulses);
}
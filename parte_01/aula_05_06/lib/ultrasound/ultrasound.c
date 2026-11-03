
#include "ultrasound.h"

#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/gpio.h>

#define TRIGGER_PULSE_NS 10000 // 10 us = 10000 ns

static const struct pwm_dt_spec trigger = PWM_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);

#define ZEPHYR_USER_NODE DT_PATH(zephyr_user)
static const struct gpio_dt_spec echo = GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, echo_gpios);

static struct gpio_callback echo_cb_data;

K_MSGQ_DEFINE(ultrasound_msgq, sizeof(uint32_t), 10, 4);

static atomic_t threshold_distance_cm = ATOMIC_INIT(0);
static struct k_sem *threshold_reached_sem = NULL;

void echo_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins) 
{
    static uint32_t last_capture = 0;
    
    uint32_t current_capture = k_cycle_get_32();
    
    if (gpio_pin_get_dt(&echo)) 
    {
        last_capture = current_capture;
    } 
    else 
    {    uint32_t duration = current_capture - last_capture;
        
        // 2 cm de distância mínima para 12 MHz equivale a cerca de 1380 ticks.
        if (duration > 680)
            k_msgq_put(&ultrasound_msgq, &duration, K_NO_WAIT);
    }
}

uint8_t config_sensor()
{
    if (!pwm_is_ready_dt(&trigger)) 
    {
        printk("Erro: Dispositivo pwm não está pronto!\n");
        return -1;
    }

    if (pwm_set_pulse_dt(&trigger, TRIGGER_PULSE_NS))
        printk("Erro ao configurar o pulso do pwm\n");

    gpio_pin_configure_dt(&echo, GPIO_INPUT);
    int error = gpio_pin_interrupt_configure_dt(&echo, GPIO_INT_EDGE_BOTH);
    if (error < 0)
    {
        printk("Erro ao configurar interrupção, %d\n", error);
        return -1;
    }
    gpio_init_callback(&echo_cb_data, echo_isr, (1 << echo.pin));
    gpio_add_callback(echo.port, &echo_cb_data);

    return 0;
}

uint32_t sensor_read_distance_cm(uint32_t pulse_duration_ticks)
{
    // c = 340 m/s = 340 * 100 cm / 1000000us = 34 cm/us
    // considerando ida e volta temos:
    //      c = 2 * x / t => x = c * t / 2 = t / (2 / c) = t / 58.8 cm
    // 
    // isso se a leitura de  sensor_read_ticks() fosse dada em us, mas é dada em ticks
    // de 1 / (48 / 8) uS => 1 / 6 us
    uint32_t duration_us = k_cyc_to_us_floor32(pulse_duration_ticks);

    return duration_us / 58;
}

void ultrasound_set_threshold_notify(uint32_t distance_cm, struct k_sem *sem)
{
    threshold_reached_sem = sem;
    atomic_set(&threshold_distance_cm, distance_cm);
}

void ultrasound_entry_point(void *p1, void *p2, void *p3)
{
    if (config_sensor())
        return;

    uint32_t duration;

    while (1)
    {
        k_msgq_get(&ultrasound_msgq, &duration, K_FOREVER);
        uint32_t distance_cm = sensor_read_distance_cm(duration);

        uint32_t threshold = (uint32_t)atomic_get(&threshold_distance_cm);
        if (threshold > 0 && threshold_reached_sem != NULL && distance_cm <= threshold)
        {
            k_sem_give(threshold_reached_sem);
        }
    }
}

K_THREAD_DEFINE(ultrasound_tid, 512,
                ultrasound_entry_point, NULL, NULL, NULL,
                2, 0, 0);



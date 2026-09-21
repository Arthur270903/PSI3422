#include "bridge_h.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_motor_control, LOG_LEVEL_INF);

#define ZEPHYR_USER_NODE DT_PATH(zephyr_user)
static const struct gpio_dt_spec out1 = GPIO_DT_SPEC_GET_BY_IDX(ZEPHYR_USER_NODE, pins_control_gpios, 0);
static const struct gpio_dt_spec out2 = GPIO_DT_SPEC_GET_BY_IDX(ZEPHYR_USER_NODE, pins_control_gpios, 1);
static const struct gpio_dt_spec out3 = GPIO_DT_SPEC_GET_BY_IDX(ZEPHYR_USER_NODE, pins_control_gpios, 2);
static const struct gpio_dt_spec out4 = GPIO_DT_SPEC_GET_BY_IDX(ZEPHYR_USER_NODE, pins_control_gpios, 3);

static const struct pwm_dt_spec pwm_motor_left  = PWM_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 1);
static const struct pwm_dt_spec pwm_motor_right = PWM_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 2);

int bridge_h_init(void) 
{
    if (!gpio_is_ready_dt(&out1) || !gpio_is_ready_dt(&out2) ||
        !gpio_is_ready_dt(&out3) || !gpio_is_ready_dt(&out4)) 
    {
        return -ENODEV;
    }

    gpio_pin_configure_dt(&out1, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&out2, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&out3, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&out4, GPIO_OUTPUT_INACTIVE);

    return 0;
}

void bridge_h_front() 
{
    gpio_pin_set_dt(&out1, 1);
    gpio_pin_set_dt(&out2, 0);
    gpio_pin_set_dt(&out3, 1);
    gpio_pin_set_dt(&out4, 0);
}

void bridge_h_back() 
{
    gpio_pin_set_dt(&out1, 0);
    gpio_pin_set_dt(&out2, 1);
    gpio_pin_set_dt(&out3, 0);
    gpio_pin_set_dt(&out4, 1);
}

void bridge_h_left() 
{
    gpio_pin_set_dt(&out1, 0);
    gpio_pin_set_dt(&out2, 1);
    gpio_pin_set_dt(&out3, 1);
    gpio_pin_set_dt(&out4, 0);
}

void bridge_h_right() 
{
    gpio_pin_set_dt(&out1, 1);
    gpio_pin_set_dt(&out2, 0);
    gpio_pin_set_dt(&out3, 0);
    gpio_pin_set_dt(&out4, 1);
}

void bridge_h_stop() 
{
    gpio_pin_set_dt(&out1, 0);
    gpio_pin_set_dt(&out2, 0);
    gpio_pin_set_dt(&out3, 0);
    gpio_pin_set_dt(&out4, 0);
}

int bridge_h_set_speed(uint8_t left, uint8_t right)
{
    if (left > 100 || right > 100)
        return -EINVAL;

    uint32_t pulse_left  = (pwm_motor_left.period * left)  / 100;
    uint32_t pulse_right = (pwm_motor_right.period * right) / 100;

    int error = pwm_set_pulse_dt(&pwm_motor_left, pulse_left);
    if (error) 
    {
        LOG_ERR("Erro ao configurar PWM esquerdo: %d\n", error);
        return error;
    }

    error = pwm_set_pulse_dt(&pwm_motor_right, pulse_right);
    if (error) 
    {
        LOG_ERR("Erro ao configurar PWM direito: %d\n", error);
        return error;
    }

    return 0;
}
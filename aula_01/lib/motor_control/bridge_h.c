#include "bridge_h.h"

// Mapeia os pinos definidos no Devicetree
static const struct gpio_dt_spec out1 = GPIO_DT_SPEC_GET(DT_ALIAS(out1), gpios);
static const struct gpio_dt_spec out2 = GPIO_DT_SPEC_GET(DT_ALIAS(out2), gpios);
static const struct gpio_dt_spec out3 = GPIO_DT_SPEC_GET(DT_ALIAS(out3), gpios);
static const struct gpio_dt_spec out4 = GPIO_DT_SPEC_GET(DT_ALIAS(out4), gpios);

int brdge_h_init(void) 
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

void brdge_h_front() 
{
    gpio_pin_set_dt(&out1, 1);
    gpio_pin_set_dt(&out2, 0);
    gpio_pin_set_dt(&out3, 1);
    gpio_pin_set_dt(&out4, 0);
}

void brdge_h_back() 
{
    gpio_pin_set_dt(&out1, 0);
    gpio_pin_set_dt(&out2, 1);
    gpio_pin_set_dt(&out3, 0);
    gpio_pin_set_dt(&out4, 1);
}

void brdge_h_left() 
{
    gpio_pin_set_dt(&out1, 0);
    gpio_pin_set_dt(&out2, 1);
    gpio_pin_set_dt(&out3, 1);
    gpio_pin_set_dt(&out4, 0);
}

void brdge_h_right() 
{
    gpio_pin_set_dt(&out1, 1);
    gpio_pin_set_dt(&out2, 0);
    gpio_pin_set_dt(&out3, 0);
    gpio_pin_set_dt(&out4, 1);
}

void brdge_h_stop() 
{
    gpio_pin_set_dt(&out1, 0);
    gpio_pin_set_dt(&out2, 0);
    gpio_pin_set_dt(&out3, 0);
    gpio_pin_set_dt(&out4, 0);
}
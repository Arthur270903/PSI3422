#include "ponte_h.h"

// Mapeia os pinos definidos no Devicetree
static const struct gpio_dt_spec in1 = GPIO_DT_SPEC_GET(DT_ALIAS(in1), gpios);
static const struct gpio_dt_spec in2 = GPIO_DT_SPEC_GET(DT_ALIAS(in2), gpios);
static const struct gpio_dt_spec in3 = GPIO_DT_SPEC_GET(DT_ALIAS(in3), gpios);
static const struct gpio_dt_spec in4 = GPIO_DT_SPEC_GET(DT_ALIAS(in4), gpios);

int ponte_h_init(void) {
    if (!gpio_is_ready_dt(&in1) || !gpio_is_ready_dt(&in2) ||
        !gpio_is_ready_dt(&in3) || !gpio_is_ready_dt(&in4)) {
        return -ENODEV;
    }

    gpio_pin_configure_dt(&in1, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&in2, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&in3, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&in4, GPIO_OUTPUT_INACTIVE);

    return 0;
}

void ponte_h_frente(void) {
    gpio_pin_set_dt(&in1, 1);
    gpio_pin_set_dt(&in2, 0);
    gpio_pin_set_dt(&in3, 1);
    gpio_pin_set_dt(&in4, 0);
}

void ponte_h_tras(void) {
    gpio_pin_set_dt(&in1, 0);
    gpio_pin_set_dt(&in2, 1);
    gpio_pin_set_dt(&in3, 0);
    gpio_pin_set_dt(&in4, 1);
}

void ponte_h_esquerda(void) {
    gpio_pin_set_dt(&in1, 0);
    gpio_pin_set_dt(&in2, 1);
    gpio_pin_set_dt(&in3, 1);
    gpio_pin_set_dt(&in4, 0);
}

void ponte_h_direita(void) {
    gpio_pin_set_dt(&in1, 1);
    gpio_pin_set_dt(&in2, 0);
    gpio_pin_set_dt(&in3, 0);
    gpio_pin_set_dt(&in4, 1);
}

void ponte_h_parar(void) {
    gpio_pin_set_dt(&in1, 0);
    gpio_pin_set_dt(&in2, 0);
    gpio_pin_set_dt(&in3, 0);
    gpio_pin_set_dt(&in4, 0);
}
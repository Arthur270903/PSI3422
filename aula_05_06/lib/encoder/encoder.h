#ifndef ENCODER_H
#define ENCODER_H

#include <zephyr/kernel.h>

void encoders_init();
void encoder_reset();
uint32_t encoder_get_avg_pulses();
uint32_t encoder_get_left_pulses();
uint32_t encoder_get_right_pulses();

void encoder_set_target_notify(uint32_t pulses, struct k_sem *sem);

#endif /* ENCODER_H */
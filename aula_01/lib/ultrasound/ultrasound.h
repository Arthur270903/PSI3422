/// hc-sr04
///
/// 10 us pulse trigger
/// Max range 400 cm
/// Min range   2 cm
/// 
/// Distância = (Tempo echo em nível alto * velocidade do som) / 2
/// 
/// teste max. (velocidade do som = 34300 * 10^6 cm/us)
///     400 = T_max * 34300 / 2 = T_max * 17150 => T_max = 400 / 17150 s
///
///     T_max = pulse_duration / f_timer = 2 * prescale * pulse_duration / f_core =>
///         => 400 / 17150 = 2 * prescale * 65536 / (48 * 10^6) =>
///         => prescale = 8.54 ~ 8
///

#ifndef ULTRASOUND_H
#define ULTRASOUND_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>

#include <stdint.h>

#include <tpm_dt.h>

uint8_t config_sensor();
void handler_sensor();
uint32_t sensor_read_distance();
 
#endif /* ULTRASOUND_H */
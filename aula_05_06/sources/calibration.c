#include "calibration.h"
#include "bridge_h.h"
#include "encoder.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_calibration, LOG_LEVEL_INF);

#define CALIBRATION_DURATION_MS  2000
#define CALIBRATION_MAX_ITERATIONS   10 // Aumentamos para dar tempo de estabilizar
#define CALIBRATION_TOLERANCE_PULSES 5  // Uma tolerância um pouco mais realista


uint8_t motor_calibrate(void)
{

    bridge_h_set_speed(80, 100);
    return 0;
}
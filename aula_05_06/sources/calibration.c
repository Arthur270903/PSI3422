#include "calibration.h"
#include "bridge_h.h"
#include "encoder.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_calibration, LOG_LEVEL_INF);

#define CALIBRATION_DURATION_MS  2000
#define CALIBRATION_MAX_ITERATIONS   10 // Aumentamos para dar tempo de estabilizar
#define CALIBRATION_TOLERANCE_PULSES 5  // Uma tolerância um pouco mais realista

static uint8_t left_scale_percent  = 100;
static uint8_t right_scale_percent = 100;

uint8_t motor_calibrate(void)
{
    left_scale_percent  = 100;
    right_scale_percent = 100;

    for (int i = 0; i < CALIBRATION_MAX_ITERATIONS; i++)
    {
        encoder_reset();
        bridge_h_set_speed(left_scale_percent, right_scale_percent);
        bridge_h_front();

        k_msleep(CALIBRATION_DURATION_MS);

        // LER OS ENCODERS ANTES DE PARAR PARA IGNORAR A INÉRCIA DA FRENAGEM
        uint32_t left_pulses  = encoder_get_left_pulses();
        uint32_t right_pulses = encoder_get_right_pulses();

        bridge_h_stop();

        LOG_INF("cal[%d]: esq=%u (scale=%u%%) dir=%u (scale=%u%%)",
                i, left_pulses, left_scale_percent, right_pulses, right_scale_percent);

        if (left_pulses == 0 || right_pulses == 0)
        {
            LOG_ERR("Calibracao falhou: encoder nao registrou pulsos");
            return 1;
        }

        int32_t diff = (int32_t)left_pulses - (int32_t)right_pulses;
        
        if (abs(diff) <= CALIBRATION_TOLERANCE_PULSES)
        {
            LOG_INF("Calibracao convergida em %d iteracoes", i + 1);
            break;
        }

        // Fator de ajuste proporcional suave. 
        // Em vez de regra de 3, tiramos uma pequena porcentagem do motor mais rápido.
        // O valor de 'kp' (ganho) pode precisar de ajuste dependendo de quantos pulsos o seu motor dá em 2s.
        // Assumindo que a diferença de pulsos seja, por exemplo, 50...
        
        float proporcao = (float)left_pulses / (float)right_pulses;

        if (left_pulses > right_pulses) {
            // Esquerda está mais rápida. Reduzimos a esquerda baseada na proporção, mas limitando o impacto.
            left_scale_percent = (uint8_t)(left_scale_percent / proporcao); 
        } else {
            // Direita está mais rápida.
            proporcao = (float)right_pulses / (float)left_pulses;
            right_scale_percent = (uint8_t)(right_scale_percent / proporcao);
        }

        // Normaliza para garantir que pelo menos um sempre fique em 100%
        uint8_t max_scale = (left_scale_percent > right_scale_percent) ? left_scale_percent : right_scale_percent;
        
        if (max_scale > 0 && max_scale < 100) {
            left_scale_percent  = (uint8_t)(((uint32_t)left_scale_percent  * 100) / max_scale);
            right_scale_percent = (uint8_t)(((uint32_t)right_scale_percent * 100) / max_scale);
        }

        k_msleep(500); // Aguarda o robô parar completamente antes do próximo teste
    }

    bridge_h_set_speed(left_scale_percent, right_scale_percent);
    return 0;
}
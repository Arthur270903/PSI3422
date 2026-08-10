
#include "ultrasound.h"

// --- trigger pwm config ---
#define PWM_TRIGGER_NODE DT_ALIAS(pwm_trigger) // pwm trigger
#define TRIGGER_PULSE_NS 10000                 // 10 us = 10000 ns

#if DT_NODE_HAS_STATUS(PWM_TRIGGER_NODE, okay)
    static const struct pwm_dt_spec trigger = PWM_DT_SPEC_GET(PWM_TRIGGER_NODE);
#else
    #error "Unsupported board."
#endif
// ---

// --- input capture mode (tpm) ---
#define TPM_TRIGGER_NODE DT_NODELABEL(tpm1)

#if DT_NODE_HAS_STATUS(TPM_TRIGGER_NODE, okay)
    static const struct tpm_dt_spec echo = TPM_DT_SPEC_GET(TPM_TRIGGER_NODE);
#else
    #error "Unsupported board"
#endif
// ---

volatile uint32_t pulse_duration_ticks = 0;

void tpm1_isr(void *arg)
{
    handler_sensor();
}

uint8_t config_sensor()
{
    // --- TODO: ---
    //    A placa frdm-kl25z não roteia o clock do pwm 
    // automaticamente no Zephyr.
    //    Configurar o registrador SIM_SOPT2 (0x40048004) com os 
    // bits (TPMSRC) em "01" (MCGFLLCLK).
    SIM->SOPT2 |= SIM_SOPT2_PLLFLLSEL_MASK | SIM_SOPT2_TPMSRC(1);
    // ---

    // --- trigger pwm ---
    if (!pwm_is_ready_dt(&trigger)) 
    {
        printk("Erro: Dispositivo pwm não está pronto!\n");
        return -1;
    }

    if (pwm_set_pulse_dt(&trigger, TRIGGER_PULSE_NS))
        printk("Erro ao configurar o pulso do pwm\n");
    // ---

    // --- input capture mode (tpm) ---
    if (!tpm_is_ready_dt(&echo)) 
    {
        printk("Erro: Dispositivo tpm1 não está pronto!\n");
        return -1;
    }

    TPM_SETUP_IRQ(TPM_TRIGGER_NODE, tpm1_isr);

    TPM1->CONTROLS[0].CnSC = TPM_CnSC_CHF_MASK | TPM_CnSC_CHIE_MASK | TPM_CnSC_ELSA_MASK | TPM_CnSC_ELSB_MASK;
    
    TPM1->SC |= TPM_SC_CMOD(1);
    // ---

    return 0;
}

void handler_sensor()
{
    static uint32_t last_capture = 0;
    
    uint32_t current_capture = TPM1->CONTROLS[0].CnV;
    
    TPM1->STATUS |= TPM_STATUS_CH0F_MASK; 

    if (PTE->PDIR & (1 << 20)) 
    {
        last_capture = current_capture;
    } 
    else 
    {      
        // Calcula a duração aplicando a máscara de 16 bits para evitar estouros negativos
        uint32_t duration = (current_capture - last_capture) & 0xFFFF;
        
        // 2 cm de distância mínima para 12 MHz equivale a cerca de 1380 ticks.
        if (duration > 680)
            pulse_duration_ticks = duration;
    }
}

uint32_t sensor_read_distance()
{
    return pulse_duration_ticks;
}
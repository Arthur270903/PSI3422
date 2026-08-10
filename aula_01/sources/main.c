#include <zephyr/kernel.h>
#include <zephyr/device.h>

#include "ultrasound.h"
#include "bridge_h.h"

#define TARGET_DISTANCE_CM 10 // TODO: test

K_SEM_DEFINE(target_sem, 0, 1);

extern void ultrasound_entry_point(void *, void *, void *)
{
    if(config_sensor())
        return;

    while (1) 
    {   
        k_sem_take(&ultrasound_sem, K_FOREVER);

        // f_timer = f_core / (2 * prescale)   [Hz]
        // pulse = pulse_duration / f_timer    [s]
        //printk("Distancia do objeto: %u cm\n", sensor_read_distance_cm());

        uint32_t distance_cm = sensor_read_distance_cm();

        if (distance_cm >= TARGET_DISTANCE_CM)
            k_sem_give(&target_sem);
    }
}

// simple motor control
// - anda para frente até achar um alvo
// - se achar um alvo ele para e vira para a direita
void motor_control_entry_point(void *, void *, void *)
{
    if(brdge_h_init())
        return;

    brdge_h_front();

    while (1) 
    {   
        k_sem_take(&target_sem, K_FOREVER);
        brdge_h_stop();  //
        k_msleep(1000);  // 
        brdge_h_right(); //
        k_msleep(1000);  //
        brdge_h_stop();  // TOOD: FIX
    }
}


K_THREAD_DEFINE(motor_control_tid, 512,
                motor_control_entry_point, NULL, NULL, NULL,
                0, 0, 0);

// ---
int main()
{
    while (1) 
    {
        k_sleep(K_FOREVER); 
    }

    return 0;
}


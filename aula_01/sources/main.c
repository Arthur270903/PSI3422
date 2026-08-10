#include <zephyr/kernel.h>
#include <zephyr/device.h>

#include "ultrasound.h"

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

// ---
int main()
{
    while (1) 
    {
        k_sleep(K_FOREVER); 
    }

    return 0;
}


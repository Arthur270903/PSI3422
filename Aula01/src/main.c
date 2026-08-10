#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include "ultrasound.h"
#define LIMIT_DISTANCE_CM 10 

 #define OUT1 DT_ALIAS(out1) 

   static const struct gpio_dt_spec out1 = GPIO_DT_SPEC_GET(OUT1,gpios);


// ---
int main()
{
    
    if(config_sensor())
        return -1;

    while (1) 
    {
        // f_timer = f_core / (2 * prescale)   [Hz]
        // pulse = pulse_duration / f_timer    [s]
        printk("Distancia do objeto: %u cm\n", sensor_read_distance() / (58 * 6));

        k_msleep(1000); 
    }

    return 0;
}


#ifndef TPM_DT_H
#define TPM_DT_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>

#include <stdint.h>
#include <stdbool.h>

struct tpm_dt_spec 
{
    const struct device *dev;
    uint32_t interrupt;
    uint32_t priority;
};

#define TPM_DT_SPEC_GET(node_id) \
    { \
        .dev = DEVICE_DT_GET(node_id), \
        .interrupt = DT_IRQN(node_id), \
        .priority = DT_IRQ(node_id, priority) \
    }

static inline bool tpm_is_ready_dt(const struct tpm_dt_spec *spec)
{
	return device_is_ready(spec->dev);
}

#define TPM_SETUP_IRQ(node_id, isr_function) \
    do { \
        IRQ_CONNECT(DT_IRQN(node_id), DT_IRQ(node_id, priority), isr_function, NULL, 0); \
        irq_enable(DT_IRQN(node_id)); \
    } while (0)

#endif /* TPM_DT_H */
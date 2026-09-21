#ifndef RADIO_LINK_H
#define RADIO_LINK_H

#include <zephyr/kernel.h>

typedef enum {
    COMMAND_RUN   = 'R',
    COMMAND_STOP  = 'S',
    COMMAND_PRINT = 'P',
    COMMAND_CLEAN = 'C'
} command_t;

extern struct k_msgq command_msgq;

#endif /* RADIO_LINK_H */
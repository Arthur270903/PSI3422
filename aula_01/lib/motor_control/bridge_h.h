#ifndef BRIDGE_H_H
#define BRIDGE_H_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

int  brdge_h_init();
void brdge_h_front();
void brdge_h_back();
void brdge_h_left();
void brdge_h_right();
void brdge_h_stop();

#endif /* BRIDGE_H */
#ifndef BRIDGE_H_H
#define BRIDGE_H_H

#include <zephyr/kernel.h>

int  bridge_h_init();
void bridge_h_front();
void bridge_h_back();
void bridge_h_left();
void bridge_h_right();
void bridge_h_stop();

int bridge_h_set_speed(uint8_t left, uint8_t right);

#endif /* BRIDGE_H */
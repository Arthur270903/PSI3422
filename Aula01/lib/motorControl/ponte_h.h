#ifndef PONTE_H_H_
#define PONTE_H_H_

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

int ponte_h_init(void);
void ponte_h_frente(void);
void ponte_h_tras(void);
void ponte_h_esquerda(void);
void ponte_h_direita(void);
void ponte_h_parar(void);

#endif
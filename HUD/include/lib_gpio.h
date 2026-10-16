#ifndef _GPIO_H_
#define _GPIO_H_

#include "driver/gpio.h"

#define BUTTON_PIN              GPIO_NUM_10   // Chân nút bấm nhận lệnh trên HUD

#define BUTTON_READ             gpio_get_level(BUTTON_PIN)

typedef struct
{
    uint8_t   state_old;
    uint16_t  counter;
    uint8_t   flag_hold;
    uint8_t   flag_press;
    uint8_t   flag_hold_triggered;
} Button_t;

extern Button_t button;

void BUTTON_ReadAll(void);
void BUTTON_Process(Button_t *button, uint8_t state);
void GPIO_Setup(void);

#endif
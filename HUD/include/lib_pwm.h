#ifndef _LIB_PWM_H_
#define _LIB_PWM_H_


#include "driver/ledc.h"

// ESP32 LEDC (PWM) Configuration 
#define PWM_GPIO_PIN            GPIO_NUM_5   // PWM output pin
#define PWM_TIMER_NUM           LEDC_TIMER_0
#define PWM_CHANNEL_NUM         LEDC_CHANNEL_0
#define PWM_FREQUENCY           1000          // 1 kHz
#define PWM_RESOLUTION          LEDC_TIMER_10_BIT

void PWM_Setup(void);
void PWM_SetDuty(float duty);
void PWM_SetFromValue(uint16_t value, uint16_t max_value);

#endif

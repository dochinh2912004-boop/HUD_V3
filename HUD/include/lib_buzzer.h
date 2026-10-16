#ifndef _LIB_BUZZER_H_
#define _LIB_BUZZER_H_

#include "driver/gpio.h"  
#include "driver/ledc.h"

#define BUZZER_GPIO_PIN         GPIO_NUM_3  // Buzzer output pin
#define BUZZER_TIMER_NUM        LEDC_TIMER_1
#define BUZZER_CHANNEL_NUM      LEDC_CHANNEL_1
#define BUZZER_FREQUENCY        2000          // 2 kHz

void Buzzer_Setup(void);
void Buzzer_On(void);
void Buzzer_Off(void);

// Am luong coi: 0-100%. Anh huong den duty khi Buzzer_On() duoc goi. 
extern uint8_t buzzer_volume;
void Buzzer_SetVolume(uint8_t percent);      // chi cap nhat RAM (khong ghi flash)
void Buzzer_SaveVolume(uint8_t percent);     // cap nhat + ghi flash
uint8_t Buzzer_GetVolume(void);
void Buzzer_LoadVolumeFromFlash(void);       // goi luc boot

#endif // _LIB_BUZZER_H_
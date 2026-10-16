#ifndef _LIB_DELAY_H_
#define _LIB_DELAY_H_

#include "lib_gpio.h"
#include "stdbool.h"
#include "lib_led.h"
#include "freertos/FreeRTOS.h"

#define TIME_BLINK_LED          1000
#define TIME_UPDATE_PWM         10
#define TIME_SAMPLE_DATA        2000
#define TIME_SWITCH_PARTITION   5000
#define TIME_GEAR_M             500
#define TIME_WAIT_RPM           60000
#define TIME_BLINK_WARNING      500
#define TIME_TURN               375
#define TIME_UART_LIM_LUX       1000

extern volatile bool flag_blink_led;
extern volatile bool flag_update_pwm;
extern volatile bool flag_switch_partition;
extern volatile bool flag_sample_data;
extern volatile bool flag_GEAR_M;
extern volatile bool flag_BLINK_WARNING;
extern volatile bool flag_Reset_RPM;
extern volatile bool flag_Reset_Turn;
extern uint32_t COUNT_TIME_UART_LIM_LUX;


extern volatile uint32_t count_clock;

void Delay_Setup(void);
void Delay_ms(uint16_t time_delay);

#endif // _LIB_DELAY_H_
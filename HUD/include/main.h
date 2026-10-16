#ifndef _MAIN_H_
#define _MAIN_H_

#include "lib_pwm.h"
#include "lib_led.h"
#include "lib_adc.h"
#include "lib_gpio.h"
#include "lib_delay.h"
#include "lib_canbus.h"
#include "lib_buzzer.h"

#define PHOTORESISTOR_MIN		41
#define PHOTORESISTOR_MAX		3276

// The max ADC is 3276 when measure in reality
#define ADC_MAX					3276.0f

#define PERCENT_TO_ADC(X)       ((X*ADC_MAX)/100)    	// X is brightness %
#define LIMIT_PERCENT_MIN		PERCENT_TO_ADC(5)   	// 5%
#define LIMIT_PERCENT_MAX		PERCENT_TO_ADC(40)		// 40%

void SYS_Blink_Led(void);
void Adjust_Brightness(void);
void Read_Config_From_Flash(void);

#endif 

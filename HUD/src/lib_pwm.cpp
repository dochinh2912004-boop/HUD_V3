#include "lib_pwm.h"
#include "lib_led.h"
#include <string.h>
static uint32_t pulse = 0;
static uint32_t pwm_period = 1023; /* 10-bit resolution */

void PWM_Setup(void)
{
	// 1. Cấu hình Timer cho PWM (Dùng memset xóa rác chặn đứng lỗi chân 88)
    ledc_timer_config_t ledc_timer;
    memset(&ledc_timer, 0, sizeof(ledc_timer));

    // ledc_timer.speed_mode       = LEDC_HIGH_SPEED_MODE;
    ledc_timer.timer_num        = PWM_TIMER_NUM;
    ledc_timer.duty_resolution  = PWM_RESOLUTION;
    ledc_timer.freq_hz          = PWM_FREQUENCY;
    ledc_timer.clk_cfg          = LEDC_AUTO_CLK;

    ledc_timer_config(&ledc_timer);

    // 2. Cấu hình Channel cho PWM (Dùng memset xóa rác)
    ledc_channel_config_t ledc_channel;
    memset(&ledc_channel, 0, sizeof(ledc_channel));

    ledc_channel.speed_mode     = LEDC_LOW_SPEED_MODE;
    ledc_channel.channel        = PWM_CHANNEL_NUM;
    ledc_channel.timer_sel      = PWM_TIMER_NUM;
    ledc_channel.gpio_num       = PWM_GPIO_PIN;
    ledc_channel.duty           = 0;
    ledc_channel.hpoint         = 0;
    // Ép driver tắt tính năng đảo pha đầu ra (Ngăn chặn việc driver tự cấu hình trở kéo nhầm chân)
    ledc_channel.flags.output_invert = 0; 

    ledc_channel_config(&ledc_channel);
}

void PWM_SetDuty(float duty)
{
	if (duty < 0.0f)
	{
		duty = 0.0f;
	}
	else if (duty > 100.0f)
	{
		duty = 100.0f;
	}

	// pulse = (uint32_t)((duty * pwm_period) / 100.0f);
    uint32_t on_duty = (uint32_t)((duty * pwm_period) / 100.0f);
	pulse = pwm_period - on_duty;   // đảo lại vì OE active-low
    
	ledc_set_duty(LEDC_LOW_SPEED_MODE, PWM_CHANNEL_NUM, pulse);
	ledc_update_duty(LEDC_LOW_SPEED_MODE, PWM_CHANNEL_NUM);
}

void PWM_SetFromValue(uint16_t value, uint16_t max_value)
{
	float duty = ((float)value / max_value) * 100.0f;
	PWM_SetDuty(duty);
}

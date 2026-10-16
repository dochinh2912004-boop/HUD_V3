#include "lib_buzzer.h"
#include "lib_flash.h"
#include <string.h>

#define BUZZER_VOLUME_KEY   "buzzer_vol"
#define BUZZER_DUTY_MAX     1023   // LEDC_TIMER_10_BIT => 0..1023

uint8_t buzzer_volume = 50; // mac dinh 50%

void Buzzer_Setup(void)
{
    ledc_timer_config_t ledc_timer;
    memset(&ledc_timer, 0, sizeof(ledc_timer));
    ledc_timer.timer_num       = BUZZER_TIMER_NUM;
    ledc_timer.duty_resolution = LEDC_TIMER_10_BIT;
    ledc_timer.freq_hz         = BUZZER_FREQUENCY;
    ledc_timer.clk_cfg         = LEDC_AUTO_CLK;
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel;
    memset(&ledc_channel, 0, sizeof(ledc_channel));
    ledc_channel.speed_mode = LEDC_LOW_SPEED_MODE;
    ledc_channel.channel    = BUZZER_CHANNEL_NUM;
    ledc_channel.timer_sel  = BUZZER_TIMER_NUM;
    ledc_channel.gpio_num   = BUZZER_GPIO_PIN;
    ledc_channel.duty       = 0;
    ledc_channel.hpoint     = 0;
    ledc_channel_config(&ledc_channel);
}

void Buzzer_On(void)
{
    uint8_t vol = buzzer_volume;
    if (vol > 100) vol = 100;
    // Duty ~50% khi vol=100, quy doi tuyen tinh de tang/giam theo % nguoi dung chon
    uint32_t duty = ((uint32_t)BUZZER_DUTY_MAX / 2) * vol / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BUZZER_CHANNEL_NUM, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BUZZER_CHANNEL_NUM);
}

void Buzzer_Off(void)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BUZZER_CHANNEL_NUM, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BUZZER_CHANNEL_NUM);
}

void Buzzer_SetVolume(uint8_t percent)
{
    if (percent > 100) percent = 100;
    buzzer_volume = percent;
}

void Buzzer_SaveVolume(uint8_t percent)
{
    Buzzer_SetVolume(percent);
    Flash_Write_uint8_t(BUZZER_VOLUME_KEY, (uint8_t)(buzzer_volume + 1));
}

uint8_t Buzzer_GetVolume(void)
{
    return buzzer_volume;
}

void Buzzer_LoadVolumeFromFlash(void)
{
  uint8_t raw = Flash_Read_uint8_t(BUZZER_VOLUME_KEY);
    if (raw == 0) {
        buzzer_volume = 50;   // that su chua tung luu -> mac dinh
    } else {
        buzzer_volume = (uint8_t)(raw - 1);  // da luu, kha nang 0..100
    }
}
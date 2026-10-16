#ifndef WEB_CONFIG_H_
#define WEB_CONFIG_H_

#include <stdint.h>
#include <stdbool.h>

void WebConfig_Start(void);

void WiFi_Enable(void); // Wifi khoi dong
void WiFi_Disable(void); // Wifi tat

/* Bat/tat che do tu dong chinh do sang LED theo cam bien anh sang (ADC).
 * Khi false, Adjust_Brightness() trong main.cpp se khong tu dong chinh nua,
 * giu nguyen do sang hien tai (PWM duty khong doi). */
extern volatile bool flag_led_auto_brightness;
void LedAuto_SetEnable(bool enable);       // chi cap nhat RAM
void LedAuto_SaveEnable(bool enable);      // cap nhat + ghi flash
void LedAuto_LoadFromFlash(void);          // goi luc boot

/* Muc sang co dinh khi TAT "Do sang LED tu dong": 0=Thap(15%) 1=Vua(40%) 2=Cao(70%) */
extern volatile uint8_t led_manual_level;
uint8_t LedManualLevel_GetDutyPercent(void);       // tra ve % duty ung voi muc dang chon (goi trong Adjust_Brightness)
void LedManualLevel_SetLevel(uint8_t level);       // chi cap nhat RAM
void LedManualLevel_SaveLevel(uint8_t level);      // cap nhat + ghi flash (goi tu save_handler)
void LedManualLevel_LoadFromFlash(void);           // goi luc boot (Read_Config_From_Flash)

#endif
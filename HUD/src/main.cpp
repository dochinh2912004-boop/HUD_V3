#include "main.h"
#include "nvs_flash.h"
#include "lib_ble.h"
#include "web_config.h"
#include "lib_buzzer.h"
#include "esp_log.h"


extern "C" void app_main(void)
{
    vTaskPrioritySet(NULL, 5);
    // KHỞI TẠO BỘ NHỚ NVS CHO ESP32 
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
	/* SysTick Init */
	Delay_Setup();
	/* GPIO Init */
	GPIO_Setup();
	
	ADC1_Setup();
	/* PWM Init */
	PWM_Setup();
     // buzzer
    Buzzer_Setup();
	/* LED Init */
	LED_Setup();
	/* LED brightness */
	PWM_SetDuty(5);
	// /* Display Full Led */
	LED_TestAll();
	// // /* Get Car Model */
	Read_Config_From_Flash();
    // ble
    wyn_ble_peripheral_init();
    // web config
	WebConfig_Start();
    vTaskDelay(pdMS_TO_TICKS(800)); // cho WiFi AP + HTTP server khoi dong on dinh

	/* CAN1 Init - chay SAU KHI WiFi da on dinh */
	CAN1_Setup();

    vTaskDelay(pdMS_TO_TICKS(500));

	while(1)
	{ 
        LED_Setup_HUD();

	    if(flag_display_setup == 1)         LED_Setup_CarModel();

		else if(flag_display_setup == 2)    LED_Full_On();

        else                                LED_Control();   // luôn vẽ lại CAN mỗi vòng
        
        if(Led_EN) {
            if (g_speed_limit_valid) {
 
            if ((HUD_GetDisplayMask() & HUD_REGION_SPEED_LIMIT) != 0) {
                SM16206_Speed_Limit((uint8_t)g_speed_limit_kmh);       
            }
        }
        SM16206_Camera(camera_led_should_be_on() ? 1 : 0);       // giữ/tắt icon camera theo hold timer
        SM16206_NoPassing(no_overtaking_led_should_be_on() ? 1 : 0); // giữ/tắt icon cấm vượt theo trạng thái case 30/31
        }   

		Adjust_Brightness();
		prev_carmodel_count = carmodel_count;
        vTaskDelay(pdMS_TO_TICKS(10));
	}
}

void Adjust_Brightness(void) {
    static uint16_t photoresistor     = (uint16_t)PERCENT_TO_ADC(20); // mặc định 20%
    static uint16_t photoresistor_old = (uint16_t)PERCENT_TO_ADC(20); // bắt đầu ở 20%

    if (!flag_led_auto_brightness) {
        uint8_t manual_percent = LedManualLevel_GetDutyPercent(); // 5 / 20 / 40 tuy muc da chon
    PWM_SetDuty(manual_percent);
    photoresistor_old = (uint16_t)PERCENT_TO_ADC(manual_percent);
    return;
    }

    /* Đọc ADC mỗi 2 giây */
    if (flag_sample_data == 1) {
        flag_sample_data = 0;
        photoresistor = ADC1_Read();
    }

    // Dưới ngưỡng tối thiểu → giữ 5%
    if (photoresistor < LIMIT_PERCENT_MIN) {
        PWM_SetDuty(5);
        photoresistor_old = (uint16_t)LIMIT_PERCENT_MIN;
        return;
    }
    
    //Trên ngưỡng tối đa → giữ 50% 
    if (photoresistor > LIMIT_PERCENT_MAX) {
        PWM_SetDuty(50);
        photoresistor_old = (uint16_t)LIMIT_PERCENT_MAX;
        return;
    }
    if (flag_update_pwm == 1) {
        flag_update_pwm = 0;
        if (photoresistor_old < photoresistor) photoresistor_old++;
        else if (photoresistor_old > photoresistor) photoresistor_old--;
        PWM_SetFromValue(photoresistor_old, (uint16_t)ADC_MAX);
    }
}

void Read_Config_From_Flash(void)
{
    carmodel_count = Flash_Read_uint8_t(KEY_CAR_MODEL);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    display_partition_warning = Flash_Read_uint8_t(KEY_PART_WARNING);
    vTaskDelay(pdMS_TO_TICKS(10));
    display_partition_battery = Flash_Read_uint8_t(KEY_PART_BATTERY);
    display_partition_battery_from_flash = display_partition_battery;   // <-- THÊM DÒNG NÀY

    vTaskDelay(pdMS_TO_TICKS(10));
    display_partition_time = Flash_Read_uint8_t(KEY_PART_TIME);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    display_partition_gear = Flash_Read_uint8_t(KEY_PART_GEAR);
    vTaskDelay(pdMS_TO_TICKS(10));

    display_partition_range = Flash_Read_uint8_t(KEY_PART_RANGE);
    vTaskDelay(pdMS_TO_TICKS(10));

    display_partition_driveMode = Flash_Read_uint8_t(KEY_PART_DRIVE_MODE);
    display_partition_driveMode_from_flash = display_partition_driveMode; // <-- THÊM DÒNG NÀY

    vTaskDelay(pdMS_TO_TICKS(10));

    {
        nvs_handle_t h;
        bool key_exists = false;
        esp_err_t e_open = nvs_open("hud_config", NVS_READONLY, &h);
        if (e_open == ESP_OK) {
            uint8_t tmp = 0;
            key_exists = (nvs_get_u8(h, "hud_mask_lo", &tmp) == ESP_OK);
            nvs_close(h);
        }

        if (key_exists) {
            uint8_t lo = Flash_Read_uint8_t("hud_mask_lo");
            uint8_t hi = Flash_Read_uint8_t("hud_mask_hi");
            uint16_t mask = ((uint16_t)hi << 8) | lo;
            HUD_SetDisplayMask(mask);
        } else {
            ESP_LOGW("BOOT", "Chua tung luu display_region_mask, dung mac dinh ALL");
            HUD_SetDisplayMask(HUD_REGION_MASK_ALL);
        }

        printf(">>> hud_display_mask after set: 0x%04X\n", HUD_GetDisplayMask());
    }

    if (carmodel_count == LUX || carmodel_count == FADIL || carmodel_count == LYNKCO1 || carmodel_count == LYNKCO6)
    {
        flag_Reset_RPM = 1;
    }
    else 
    {
        flag_Reset_RPM = 0;
    }

    // Nap them: am luong coi + co bat/tat LED tu dong (dung cho web config moi) 
    Buzzer_LoadVolumeFromFlash();
    LedAuto_LoadFromFlash();
    LedManualLevel_LoadFromFlash();
}
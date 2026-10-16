#ifndef _LIBFLASH_H_
#define _LIBFLASH_H_
#include <cstdint>


#define KEY_CAR_MODEL           "car_model"
#define KEY_PART_WARNING        "warning"
#define KEY_PART_BATTERY        "battery"
#define KEY_PART_TIME           "time"
#define KEY_PART_GEAR           "gear"
#define KEY_PART_RANGE          "range"
#define KEY_PART_DRIVE_MODE     "drivemode"
#define KEY_DISPLAY_REGION_MASK "disp_reg_mask"

void Flash_Lock(void);
void Flash_UnLock(void);
void Flash_Erase(uint32_t page_address);
void Flash_Write_uint8_t(const char* key, uint8_t data);
uint8_t Flash_Read_uint8_t(const char* key);

void LED_Partition_Range(void);
void LED_Partition_Gear(void);
void LED_Partition_Time(void);
void LED_Partition_Battery(void);
void LED_Partition_Warning(void);
void LED_Setup_HUD(void);
void LED_Partition(void);


#endif

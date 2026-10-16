#ifndef _LIB_LED_H_
#define _LIB_LED_H_


#include "lib_delay.h"
#include "lib_canbus.h"
#include "lib_flash.h"
#include "stdbool.h"

#define LED_SCLK_PIN 						GPIO_NUM_6 	// Clock pin (SCLK)
#define LED_LE_PIN 						    GPIO_NUM_4 	// Latch pin (RCLK)
#define LED_SDI_PIN 						GPIO_NUM_7 	// Data pin (DIO)
#define LED_OE_PIN                          GPIO_NUM_5  

#define DS_OE_H         					gpio_set_level(LED_OE_PIN, 1)
#define DS_OE_L         					gpio_set_level(LED_OE_PIN, 0)

#define DS_LE_H         					gpio_set_level(LED_LE_PIN, 1)
#define DS_LE_L         					gpio_set_level(LED_LE_PIN, 0)

#define DS_SCLK_H       					gpio_set_level(LED_SCLK_PIN, 1)
#define DS_SCLK_L       					gpio_set_level(LED_SCLK_PIN, 0)

#define DS_SDI_H        					gpio_set_level(LED_SDI_PIN, 1)
#define DS_SDI_L        					gpio_set_level(LED_SDI_PIN, 0)

#define DS_Pin_0      						0x0001  // (1 << 0)
#define DS_Pin_1      						0x0002  // (1 << 1)
#define DS_Pin_2      						0x0004  // (1 << 2)
#define DS_Pin_3      						0x0008  // (1 << 3)
#define DS_Pin_4      						0x0010  // (1 << 4)
#define DS_Pin_5      						0x0020  // (1 << 5)
#define DS_Pin_6      						0x0040  // (1 << 6)
#define DS_Pin_7      						0x0080  // (1 << 7)
#define DS_Pin_8      						0x0100  // (1 << 8)
#define DS_Pin_9      						0x0200  // (1 << 9)
#define DS_Pin_10     						0x0400  // (1 << 10)
#define DS_Pin_11     						0x0800  // (1 << 11)
#define DS_Pin_12     						0x1000  // (1 << 12)
#define DS_Pin_13     						0x2000  // (1 << 13)
#define DS_Pin_14     						0x4000  // (1 << 14)
#define DS_Pin_15     						0x8000  // (1 << 15)

#define FRAMEBUFFER_LED_SIZE				9


/*-------------------------- Warning --------------------------*/

#define DS_WARNING_DOOR_OPEN_PIN					DS_Pin_10
#define DS_WARNING_DOOR_OPEN_SEG					3
#define DS_WARNING_SEATBELT_PIN						DS_Pin_9
#define DS_WARNING_SEATBELT_SEG						3
#define DS_WARNING_TPMS_PIN							DS_Pin_8
#define DS_WARNING_TPMS_SEG							3
#define DS_WARNING_HANDBRAKE_PIN					DS_Pin_7
#define DS_WARNING_HANDBRAKE_SEG					3


/*-------------------------- Lamp --------------------------*/
#define DS_LAMP_HIGH_BEAM_PIN							DS_Pin_6
#define DS_LAMP_HIGH_BEAM_SEG							3
#define DS_LAMP_LOW_BEAM_PIN							DS_Pin_5
#define DS_LAMP_LOW_BEAM_SEG							3



/*-------------------------- Turn Left, Turn Right --------------------------*/
#define DS_LEFT_TURN_SIGNAL_PIN 					DS_Pin_4
#define DS_LEFT_TURN_SIGNAL_SEG 					3
#define DS_RIGHT_TURN_SIGNAL_PIN 					DS_Pin_11
#define DS_RIGHT_TURN_SIGNAL_SEG 					3


/*-------------------------- Gear --------------------------*/
#define DS_GEAR_P_PIN									DS_Pin_12
#define DS_GEAR_P_SEG									3
#define DS_GEAR_R_PIN									DS_Pin_13
#define DS_GEAR_R_SEG									3
#define DS_GEAR_N_PIN									DS_Pin_14
#define DS_GEAR_N_SEG									3
#define DS_GEAR_D_PIN									DS_Pin_15
#define DS_GEAR_D_SEG									3



/*-------------------------- Speed KMH --------------------------*/
#define DS_NUMBER_ONE_KM1_PIN						DS_Pin_14 | DS_Pin_15 
#define DS_NUMBER_ONE_KM1_SEG						1
#define DS_NUMBER_ONE_KM2_PIN						DS_Pin_14 | DS_Pin_15 
#define DS_NUMBER_ONE_KM2_SEG                       2

#define DS_SPEED_KMH_DOZENS_A1_PIN					DS_Pin_7
#define DS_SPEED_KMH_DOZENS_A1_SEG					1
#define DS_SPEED_KMH_DOZENS_B1_PIN					DS_Pin_8 
#define DS_SPEED_KMH_DOZENS_B1_SEG					1
#define DS_SPEED_KMH_DOZENS_C1_PIN					DS_Pin_9 
#define DS_SPEED_KMH_DOZENS_C1_SEG					1
#define DS_SPEED_KMH_DOZENS_D1_PIN					DS_Pin_10 
#define DS_SPEED_KMH_DOZENS_D1_SEG					1
#define DS_SPEED_KMH_DOZENS_E1_PIN					DS_Pin_11 
#define DS_SPEED_KMH_DOZENS_E1_SEG					1
#define DS_SPEED_KMH_DOZENS_F1_PIN					DS_Pin_12
#define DS_SPEED_KMH_DOZENS_F1_SEG					1
#define DS_SPEED_KMH_DOZENS_G1_PIN					DS_Pin_13 
#define DS_SPEED_KMH_DOZENS_G1_SEG					1
#define DS_SPEED_KMH_DOZENS_A2_PIN					DS_Pin_7 
#define DS_SPEED_KMH_DOZENS_A2_SEG					2
#define DS_SPEED_KMH_DOZENS_B2_PIN					DS_Pin_8
#define DS_SPEED_KMH_DOZENS_B2_SEG					2
#define DS_SPEED_KMH_DOZENS_C2_PIN					DS_Pin_9 
#define DS_SPEED_KMH_DOZENS_C2_SEG					2
#define DS_SPEED_KMH_DOZENS_D2_PIN					DS_Pin_10 
#define DS_SPEED_KMH_DOZENS_D2_SEG					2
#define DS_SPEED_KMH_DOZENS_E2_PIN					DS_Pin_11
#define DS_SPEED_KMH_DOZENS_E2_SEG					2
#define DS_SPEED_KMH_DOZENS_F2_PIN					DS_Pin_12
#define DS_SPEED_KMH_DOZENS_F2_SEG					2
#define DS_SPEED_KMH_DOZENS_G2_PIN					DS_Pin_13 
#define DS_SPEED_KMH_DOZENS_G2_SEG					2

#define DS_SPEED_KMH_UNIT_A1_PIN					DS_Pin_0 
#define DS_SPEED_KMH_UNIT_A1_SEG					1
#define DS_SPEED_KMH_UNIT_B1_PIN					DS_Pin_1
#define DS_SPEED_KMH_UNIT_B1_SEG					1
#define DS_SPEED_KMH_UNIT_C1_PIN					DS_Pin_2 
#define DS_SPEED_KMH_UNIT_C1_SEG					1
#define DS_SPEED_KMH_UNIT_D1_PIN					DS_Pin_3 
#define DS_SPEED_KMH_UNIT_D1_SEG					1
#define DS_SPEED_KMH_UNIT_E1_PIN					DS_Pin_4 
#define DS_SPEED_KMH_UNIT_E1_SEG					1
#define DS_SPEED_KMH_UNIT_F1_PIN					DS_Pin_5 
#define DS_SPEED_KMH_UNIT_F1_SEG					1
#define DS_SPEED_KMH_UNIT_G1_PIN					DS_Pin_6 
#define DS_SPEED_KMH_UNIT_G1_SEG					1
#define DS_SPEED_KMH_UNIT_A2_PIN					DS_Pin_0 
#define DS_SPEED_KMH_UNIT_A2_SEG					2
#define DS_SPEED_KMH_UNIT_B2_PIN					DS_Pin_1 
#define DS_SPEED_KMH_UNIT_B2_SEG					2
#define DS_SPEED_KMH_UNIT_C2_PIN					DS_Pin_2 
#define DS_SPEED_KMH_UNIT_C2_SEG					2
#define DS_SPEED_KMH_UNIT_D2_PIN					DS_Pin_3 
#define DS_SPEED_KMH_UNIT_D2_SEG					2
#define DS_SPEED_KMH_UNIT_E2_PIN					DS_Pin_4 
#define DS_SPEED_KMH_UNIT_E2_SEG					2
#define DS_SPEED_KMH_UNIT_F2_PIN					DS_Pin_5 
#define DS_SPEED_KMH_UNIT_F2_SEG					2
#define DS_SPEED_KMH_UNIT_G2_PIN					DS_Pin_6 
#define DS_SPEED_KMH_UNIT_G2_SEG					2


/*-------------------------- Navigate --------------------------*/
// #define DS_NAVIGATE_DOT_PIN							DS_Pin_8
// #define DS_NAVIGATE_DOT_SEG							5
// #define DS_NAVIGATE_STRAIGHT_PIN					DS_Pin_12 | DS_Pin_4
// #define DS_NAVIGATE_STRAIGHT_SEG					5
// #define DS_NAVIGATE_STRAIGHT_LEFT_PIN				DS_Pin_12 | DS_Pin_5
// #define DS_NAVIGATE_STRAIGHT_LEFT_SEG				5
// #define DS_NAVIGATE_STRAIGHT_RIGHT_PIN				DS_Pin_12 | DS_Pin_6
// #define DS_NAVIGATE_STRAIGHT_RIGHT_SEG				5
// #define DS_NAVIGATE_LEFT_TURN_PIN					DS_Pin_12 | DS_Pin_9
// #define DS_NAVIGATE_LEFT_TURN_SEG					5
// #define DS_NAVIGATE_RIGHT_TURN_PIN					DS_Pin_12 | DS_Pin_7
// #define DS_NAVIGATE_RIGHT_TURN_SEG					5
// #define DS_NAVIGATE_LEFT_AROUND_PIN					DS_Pin_14
// #define DS_NAVIGATE_LEFT_AROUND_SEG					5
// #define DS_NAVIGATE_RIGHT_AROUND_PIN				DS_Pin_10
// #define DS_NAVIGATE_RIGHT_AROUND_SEG				5
// #define DS_NAVIGATE_LEFT_OPPOSITE_PIN				DS_Pin_12 | DS_Pin_13
// #define DS_NAVIGATE_LEFT_OPPOSITE_SEG				5
// #define DS_NAVIGATE_RIGHT_OPPOSITE_PIN				DS_Pin_12 | DS_Pin_11
// #define DS_NAVIGATE_RIGHT_OPPOSITE_SEG				5



/*-------------------------- Speed Limit --------------------------*/
#define DS_SPEED_LIMIT_ONE_PIN						DS_Pin_13 | DS_Pin_14
#define DS_SPEED_LIMIT_ONE_SEG						4

#define DS_SPEED_LIMIT_DOZENS_A_PIN					DS_Pin_7
#define DS_SPEED_LIMIT_DOZENS_A_SEG					4
#define DS_SPEED_LIMIT_DOZENS_B_PIN					DS_Pin_8
#define DS_SPEED_LIMIT_DOZENS_B_SEG					4
#define DS_SPEED_LIMIT_DOZENS_C_PIN					DS_Pin_9
#define DS_SPEED_LIMIT_DOZENS_C_SEG					4
#define DS_SPEED_LIMIT_DOZENS_D_PIN					DS_Pin_10
#define DS_SPEED_LIMIT_DOZENS_D_SEG					4
#define DS_SPEED_LIMIT_DOZENS_E_PIN					DS_Pin_11
#define DS_SPEED_LIMIT_DOZENS_E_SEG					4
#define DS_SPEED_LIMIT_DOZENS_F_PIN					DS_Pin_12
#define DS_SPEED_LIMIT_DOZENS_F_SEG					4
#define DS_SPEED_LIMIT_DOZENS_G_PIN					DS_Pin_15
#define DS_SPEED_LIMIT_DOZENS_G_SEG					4

#define DS_SPEED_LIMIT_UNIT_A_PIN					DS_Pin_0
#define DS_SPEED_LIMIT_UNIT_A_SEG					4
#define DS_SPEED_LIMIT_UNIT_B_PIN					DS_Pin_1
#define DS_SPEED_LIMIT_UNIT_B_SEG					4
#define DS_SPEED_LIMIT_UNIT_C_PIN					DS_Pin_2
#define DS_SPEED_LIMIT_UNIT_C_SEG					4
#define DS_SPEED_LIMIT_UNIT_D_PIN					DS_Pin_3
#define DS_SPEED_LIMIT_UNIT_D_SEG					4
#define DS_SPEED_LIMIT_UNIT_E_PIN					DS_Pin_4
#define DS_SPEED_LIMIT_UNIT_E_SEG					4
#define DS_SPEED_LIMIT_UNIT_F_PIN					DS_Pin_5
#define DS_SPEED_LIMIT_UNIT_F_SEG					4
#define DS_SPEED_LIMIT_UNIT_G_PIN					DS_Pin_6
#define DS_SPEED_LIMIT_UNIT_G_SEG					4


/*-------------------------- Drive Mode --------------------------*/
#define DS_DRIVE_MODE_ECO_PIN						DS_Pin_3
#define DS_DRIVE_MODE_ECO_SEG						3
#define DS_DRIVE_MODE_NORMAL_PIN					DS_Pin_2
#define DS_DRIVE_MODE_NORMAL_SEG					3
#define DS_DRIVE_MODE_SPORT_PIN						DS_Pin_1
#define DS_DRIVE_MODE_SPORT_SEG						3
#define DS_DRIVE_MODE_COM_PIN						DS_Pin_0
#define DS_DRIVE_MODE_COM_SEG						3



/*-------------------------- Battery Percent --------------------------Checked */
#define DS_PIN_PERCENT_PIN							DS_Pin_5
#define DS_PIN_PERCENT_SEG							5

#define DS_PIN_PERCENT_ONE_PIN						DS_Pin_14 | DS_Pin_15
#define DS_PIN_PERCENT_ONE_SEG						0

#define DS_PIN_PERCENT_DOZENS_A_PIN					DS_Pin_7
#define DS_PIN_PERCENT_DOZENS_A_SEG					0
#define DS_PIN_PERCENT_DOZENS_B_PIN					DS_Pin_8
#define DS_PIN_PERCENT_DOZENS_B_SEG					0
#define DS_PIN_PERCENT_DOZENS_C_PIN					DS_Pin_9
#define DS_PIN_PERCENT_DOZENS_C_SEG					0
#define DS_PIN_PERCENT_DOZENS_D_PIN					DS_Pin_10
#define DS_PIN_PERCENT_DOZENS_D_SEG					0
#define DS_PIN_PERCENT_DOZENS_E_PIN					DS_Pin_11
#define DS_PIN_PERCENT_DOZENS_E_SEG					0
#define DS_PIN_PERCENT_DOZENS_F_PIN					DS_Pin_12
#define DS_PIN_PERCENT_DOZENS_F_SEG				    0
#define DS_PIN_PERCENT_DOZENS_G_PIN					DS_Pin_13
#define DS_PIN_PERCENT_DOZENS_G_SEG					0

#define DS_PIN_PERCENT_UNIT_A_PIN					DS_Pin_0
#define DS_PIN_PERCENT_UNIT_A_SEG					0
#define DS_PIN_PERCENT_UNIT_B_PIN					DS_Pin_1
#define DS_PIN_PERCENT_UNIT_B_SEG					0
#define DS_PIN_PERCENT_UNIT_C_PIN					DS_Pin_2
#define DS_PIN_PERCENT_UNIT_C_SEG					0
#define DS_PIN_PERCENT_UNIT_D_PIN					DS_Pin_3
#define DS_PIN_PERCENT_UNIT_D_SEG					0
#define DS_PIN_PERCENT_UNIT_E_PIN					DS_Pin_4
#define DS_PIN_PERCENT_UNIT_E_SEG					0
#define DS_PIN_PERCENT_UNIT_F_PIN					DS_Pin_5
#define DS_PIN_PERCENT_UNIT_F_SEG					0
#define DS_PIN_PERCENT_UNIT_G_PIN					DS_Pin_6
#define DS_PIN_PERCENT_UNIT_G_SEG					0


// /*-------------------------- Hours --------------------------*/
// #define DS_HOUR_PIN										DS_Pin_0 | DS_Pin_1
// #define DS_HOUR_SEG										5

// #define DS_HOUR_DOZENS_A_PIN							DS_Pin_3
// #define DS_HOUR_DOZENS_A_SEG							6
// #define DS_HOUR_DOZENS_B_PIN							DS_Pin_7
// #define DS_HOUR_DOZENS_B_SEG							6
// #define DS_HOUR_DOZENS_C_PIN							DS_Pin_5
// #define DS_HOUR_DOZENS_C_SEG							6
// #define DS_HOUR_DOZENS_D_PIN							DS_Pin_4
// #define DS_HOUR_DOZENS_D_SEG							6
// #define DS_HOUR_DOZENS_E_PIN							DS_Pin_1
// #define DS_HOUR_DOZENS_E_SEG							6
// #define DS_HOUR_DOZENS_F_PIN							DS_Pin_2
// #define DS_HOUR_DOZENS_F_SEG							6
// #define DS_HOUR_DOZENS_G_PIN							DS_Pin_6
// #define DS_HOUR_DOZENS_G_SEG							6

// #define DS_HOUR_UNIT_A_PIN								DS_Pin_11
// #define DS_HOUR_UNIT_A_SEG								6
// #define DS_HOUR_UNIT_B_PIN								DS_Pin_13
// #define DS_HOUR_UNIT_B_SEG								6
// #define DS_HOUR_UNIT_C_PIN								DS_Pin_14
// #define DS_HOUR_UNIT_C_SEG								6
// #define DS_HOUR_UNIT_D_PIN								DS_Pin_10
// #define DS_HOUR_UNIT_D_SEG								6
// #define DS_HOUR_UNIT_E_PIN								DS_Pin_8
// #define DS_HOUR_UNIT_E_SEG								6
// #define DS_HOUR_UNIT_F_PIN								DS_Pin_9
// #define DS_HOUR_UNIT_F_SEG								6
// #define DS_HOUR_UNIT_G_PIN								DS_Pin_12
// #define DS_HOUR_UNIT_G_SEG								6

// /*-------------------------- Minutes --------------------------*/
// #define DS_MINUTES_DOZENS_A_PIN						DS_Pin_4
// #define DS_MINUTES_DOZENS_A_SEG						5
// #define DS_MINUTES_DOZENS_B_PIN						DS_Pin_7
// #define DS_MINUTES_DOZENS_B_SEG						5
// #define DS_MINUTES_DOZENS_C_PIN						DS_Pin_6
// #define DS_MINUTES_DOZENS_C_SEG						5
// #define DS_MINUTES_DOZENS_D_PIN						DS_Pin_15
// #define DS_MINUTES_DOZENS_D_SEG						5
// #define DS_MINUTES_DOZENS_E_PIN						DS_Pin_2
// #define DS_MINUTES_DOZENS_E_SEG						5
// #define DS_MINUTES_DOZENS_F_PIN						DS_Pin_3
// #define DS_MINUTES_DOZENS_F_SEG						5
// #define DS_MINUTES_DOZENS_G_PIN						DS_Pin_5
// #define DS_MINUTES_DOZENS_G_SEG						5

// #define DS_MINUTES_UNIT_A_PIN							DS_Pin_10
// #define DS_MINUTES_UNIT_A_SEG							5
// #define DS_MINUTES_UNIT_B_PIN							DS_Pin_8
// #define DS_MINUTES_UNIT_B_SEG							5
// #define DS_MINUTES_UNIT_C_PIN							DS_Pin_9
// #define DS_MINUTES_UNIT_C_SEG							5
// #define DS_MINUTES_UNIT_D_PIN							DS_Pin_12
// #define DS_MINUTES_UNIT_D_SEG							5
// #define DS_MINUTES_UNIT_E_PIN							DS_Pin_14
// #define DS_MINUTES_UNIT_E_SEG							5
// #define DS_MINUTES_UNIT_F_PIN							DS_Pin_13
// #define DS_MINUTES_UNIT_F_SEG							5
// #define DS_MINUTES_UNIT_G_PIN							DS_Pin_11
// #define DS_MINUTES_UNIT_G_SEG							5


/*-------------------------- RPM KM --------------------------*/
// #define DS_RPM_KM_THOUSANDS_A_PIN    				DS_Pin_0
// #define DS_RPM_KM_THOUSANDS_A_SEG    				6
// #define DS_RPM_KM_THOUSANDS_B_PIN    				DS_Pin_1
// #define DS_RPM_KM_THOUSANDS_B_SEG    				6
// #define DS_RPM_KM_THOUSANDS_C_PIN    				DS_Pin_2
// #define DS_RPM_KM_THOUSANDS_C_SEG    				6
// #define DS_RPM_KM_THOUSANDS_D_PIN    				DS_Pin_3
// #define DS_RPM_KM_THOUSANDS_D_SEG    				6
// #define DS_RPM_KM_THOUSANDS_E_PIN    				DS_Pin_4
// #define DS_RPM_KM_THOUSANDS_E_SEG    				6
// #define DS_RPM_KM_THOUSANDS_F_PIN    				DS_Pin_5
// #define DS_RPM_KM_THOUSANDS_F_SEG    				6
// #define DS_RPM_KM_THOUSANDS_G_PIN    				DS_Pin_6
// #define DS_RPM_KM_THOUSANDS_G_SEG    				6

// #define DS_RPM_KM_HUNDREDS_A_PIN						DS_Pin_0
// #define DS_RPM_KM_HUNDREDS_A_SEG						6
// #define DS_RPM_KM_HUNDREDS_B_PIN						DS_Pin_1
// #define DS_RPM_KM_HUNDREDS_B_SEG						6
// #define DS_RPM_KM_HUNDREDS_C_PIN						DS_Pin_2
// #define DS_RPM_KM_HUNDREDS_C_SEG						6
// #define DS_RPM_KM_HUNDREDS_D_PIN						DS_Pin_3
// #define DS_RPM_KM_HUNDREDS_D_SEG						6
// #define DS_RPM_KM_HUNDREDS_E_PIN						DS_Pin_4
// #define DS_RPM_KM_HUNDREDS_E_SEG						6
// #define DS_RPM_KM_HUNDREDS_F_PIN						DS_Pin_5
// #define DS_RPM_KM_HUNDREDS_F_SEG						6
// #define DS_RPM_KM_HUNDREDS_G_PIN						DS_Pin_6
// #define DS_RPM_KM_HUNDREDS_G_SEG						6

// #define DS_RPM_KM_DOZENS_A_PIN						DS_Pin_7
// #define DS_RPM_KM_DOZENS_A_SEG						6
// #define DS_RPM_KM_DOZENS_B_PIN						DS_Pin_8
// #define DS_RPM_KM_DOZENS_B_SEG						6
// #define DS_RPM_KM_DOZENS_C_PIN						DS_Pin_9
// #define DS_RPM_KM_DOZENS_C_SEG						6
// #define DS_RPM_KM_DOZENS_D_PIN						DS_Pin_10
// #define DS_RPM_KM_DOZENS_D_SEG						6
// #define DS_RPM_KM_DOZENS_E_PIN						DS_Pin_11
// #define DS_RPM_KM_DOZENS_E_SEG						6
// #define DS_RPM_KM_DOZENS_F_PIN						DS_Pin_12
// #define DS_RPM_KM_DOZENS_F_SEG						6
// #define DS_RPM_KM_DOZENS_G_PIN						DS_Pin_13
// #define DS_RPM_KM_DOZENS_G_SEG						6

// #define DS_RPM_KM_UNIT_A_PIN							DS_Pin_14
// #define DS_RPM_KM_UNIT_A_SEG							6
// #define DS_RPM_KM_UNIT_B_PIN							DS_Pin_15
// #define DS_RPM_KM_UNIT_B_SEG							6
// #define DS_RPM_KM_UNIT_C_PIN							DS_Pin_0
// #define DS_RPM_KM_UNIT_C_SEG							7
// #define DS_RPM_KM_UNIT_D_PIN							DS_Pin_1
// #define DS_RPM_KM_UNIT_D_SEG							7
// #define DS_RPM_KM_UNIT_E_PIN							DS_Pin_2
// #define DS_RPM_KM_UNIT_E_SEG							7
// #define DS_RPM_KM_UNIT_F_PIN							DS_Pin_3
// #define DS_RPM_KM_UNIT_F_SEG							7
// #define DS_RPM_KM_UNIT_G_PIN							DS_Pin_4
// #define DS_RPM_KM_UNIT_G_SEG							7

/*-------------------------- RPM KM --------------------------*/

#define DS_KM_PIN						DS_Pin_14
#define DS_KM_SEG						5

#define DS_RPM_KM_UNIT_A_PIN						DS_Pin_0
#define DS_RPM_KM_UNIT_A_SEG						6	
#define DS_RPM_KM_UNIT_B_PIN						DS_Pin_1
#define DS_RPM_KM_UNIT_B_SEG						6
#define DS_RPM_KM_UNIT_C_PIN						DS_Pin_2
#define DS_RPM_KM_UNIT_C_SEG						6
#define DS_RPM_KM_UNIT_D_PIN						DS_Pin_3
#define DS_RPM_KM_UNIT_D_SEG						6
#define DS_RPM_KM_UNIT_E_PIN						DS_Pin_4
#define DS_RPM_KM_UNIT_E_SEG						6
#define DS_RPM_KM_UNIT_F_PIN						DS_Pin_5
#define DS_RPM_KM_UNIT_F_SEG						6
#define DS_RPM_KM_UNIT_G_PIN						DS_Pin_6
#define DS_RPM_KM_UNIT_G_SEG						6

#define DS_RPM_KM_DOZENS_A_PIN						DS_Pin_7
#define DS_RPM_KM_DOZENS_A_SEG						6
#define DS_RPM_KM_DOZENS_B_PIN						DS_Pin_8
#define DS_RPM_KM_DOZENS_B_SEG						6
#define DS_RPM_KM_DOZENS_C_PIN						DS_Pin_9
#define DS_RPM_KM_DOZENS_C_SEG						6
#define DS_RPM_KM_DOZENS_D_PIN						DS_Pin_10
#define DS_RPM_KM_DOZENS_D_SEG						6
#define DS_RPM_KM_DOZENS_E_PIN						DS_Pin_11
#define DS_RPM_KM_DOZENS_E_SEG						6
#define DS_RPM_KM_DOZENS_F_PIN						DS_Pin_12
#define DS_RPM_KM_DOZENS_F_SEG						6
#define DS_RPM_KM_DOZENS_G_PIN						DS_Pin_13
#define DS_RPM_KM_DOZENS_G_SEG						6

#define DS_RPM_KM_HUNDREDS_A_PIN							DS_Pin_0
#define DS_RPM_KM_HUNDREDS_A_SEG							5
#define DS_RPM_KM_HUNDREDS_B_PIN							DS_Pin_1
#define DS_RPM_KM_HUNDREDS_B_SEG							5
#define DS_RPM_KM_HUNDREDS_C_PIN							DS_Pin_2
#define DS_RPM_KM_HUNDREDS_C_SEG							5
#define DS_RPM_KM_HUNDREDS_D_PIN							DS_Pin_3
#define DS_RPM_KM_HUNDREDS_D_SEG							5
#define DS_RPM_KM_HUNDREDS_E_PIN							DS_Pin_4
#define DS_RPM_KM_HUNDREDS_E_SEG							5
#define DS_RPM_KM_HUNDREDS_F_PIN							DS_Pin_15
#define DS_RPM_KM_HUNDREDS_F_SEG							6
#define DS_RPM_KM_HUNDREDS_G_PIN							DS_Pin_14
#define DS_RPM_KM_HUNDREDS_G_SEG							6

/*-------------------------- Navigation --------------------------*/
// #define DS_NAVIGATION_DISTANCES_HUNDREDS_A_PIN							DS_Pin_5
// #define DS_NAVIGATION_DISTANCES_HUNDREDS_A_SEG							7
// #define DS_NAVIGATION_DISTANCES_HUNDREDS_B_PIN							DS_Pin_6
// #define DS_NAVIGATION_DISTANCES_HUNDREDS_B_SEG							7
// #define DS_NAVIGATION_DISTANCES_HUNDREDS_C_PIN							DS_Pin_7
// #define DS_NAVIGATION_DISTANCES_HUNDREDS_C_SEG							7
// #define DS_NAVIGATION_DISTANCES_HUNDREDS_D_PIN							DS_Pin_8
// #define DS_NAVIGATION_DISTANCES_HUNDREDS_D_SEG							7
// #define DS_NAVIGATION_DISTANCES_HUNDREDS_E_PIN							DS_Pin_9
// #define DS_NAVIGATION_DISTANCES_HUNDREDS_E_SEG							7
// #define DS_NAVIGATION_DISTANCES_HUNDREDS_F_PIN							DS_Pin_10
// #define DS_NAVIGATION_DISTANCES_HUNDREDS_F_SEG							7
// #define DS_NAVIGATION_DISTANCES_HUNDREDS_G_PIN							DS_Pin_11
// #define DS_NAVIGATION_DISTANCES_HUNDREDS_G_SEG							7

// #define DS_NAVIGATION_DISTANCES_DOZENS_A_PIN							DS_Pin_12
// #define DS_NAVIGATION_DISTANCES_DOZENS_A_SEG							7
// #define DS_NAVIGATION_DISTANCES_DOZENS_B_PIN							DS_Pin_13
// #define DS_NAVIGATION_DISTANCES_DOZENS_B_SEG							7
// #define DS_NAVIGATION_DISTANCES_DOZENS_C_PIN							DS_Pin_14
// #define DS_NAVIGATION_DISTANCES_DOZENS_C_SEG							7
// #define DS_NAVIGATION_DISTANCES_DOZENS_D_PIN							DS_Pin_15
// #define DS_NAVIGATION_DISTANCES_DOZENS_D_SEG							7
// #define DS_NAVIGATION_DISTANCES_DOZENS_E_PIN							DS_Pin_0
// #define DS_NAVIGATION_DISTANCES_DOZENS_E_SEG							8
// #define DS_NAVIGATION_DISTANCES_DOZENS_F_PIN							DS_Pin_1
// #define DS_NAVIGATION_DISTANCES_DOZENS_F_SEG							8
// #define DS_NAVIGATION_DISTANCES_DOZENS_G_PIN							DS_Pin_2
// #define DS_NAVIGATION_DISTANCES_DOZENS_G_SEG							8

// #define DS_NAVIGATION_DISTANCES_UNIT_A_PIN							DS_Pin_3
// #define DS_NAVIGATION_DISTANCES_UNIT_A_SEG							8
// #define DS_NAVIGATION_DISTANCES_UNIT_B_PIN							DS_Pin_4
// #define DS_NAVIGATION_DISTANCES_UNIT_B_SEG							8
// #define DS_NAVIGATION_DISTANCES_UNIT_C_PIN							DS_Pin_5
// #define DS_NAVIGATION_DISTANCES_UNIT_C_SEG							8
// #define DS_NAVIGATION_DISTANCES_UNIT_D_PIN							DS_Pin_6
// #define DS_NAVIGATION_DISTANCES_UNIT_D_SEG							8
// #define DS_NAVIGATION_DISTANCES_UNIT_E_PIN							DS_Pin_7
// #define DS_NAVIGATION_DISTANCES_UNIT_E_SEG							8
// #define DS_NAVIGATION_DISTANCES_UNIT_F_PIN							DS_Pin_8
// #define DS_NAVIGATION_DISTANCES_UNIT_F_SEG							8
// #define DS_NAVIGATION_DISTANCES_UNIT_G_PIN							DS_Pin_9
// #define DS_NAVIGATION_DISTANCES_UNIT_G_SEG							8

// #define DS_NAVIGATION_DISTANCES_DOT_PIN 							DS_Pin_10
// #define DS_NAVIGATION_DISTANCES_DOT_SEG 							8

// #define DS_NAVIGATION_DISTANCES_KM_PIN							DS_Pin_1
// #define DS_NAVIGATION_DISTANCES_KM_SEG							5
// #define DS_NAVIGATION_DISTANCES_M_PIN						    DS_Pin_2
// #define DS_NAVIGATION_DISTANCES_M_SEG 						    5
/*-------------------------- CAMERA --------------------------*/
#define DS_CAMERA_PIN							DS_Pin_6 | DS_Pin_7
#define DS_CAMERA_SEG							5

/*-------------------------- NO PASSING ( Cấm vượt ) --------------------------*/
#define DS_NOPASSING_PIN							DS_Pin_10 | DS_Pin_11
#define DS_NOPASSING_SEG							5

// #define DS_KM_PIN										DS_Pin_14
// #define DS_KM_SEG										5
// #define DS_RPM_PIN										DS_Pin_15
// #define DS_RPM_SEG										5
// #define DS_DEGREES_C_PIN 								DS_Pin_7
// #define DS_DEGREES_C_SEG								4

// #define DS_RPM_KM_DOT_HUNDREDS_PIN					DS_Pin_14
// #define DS_RPM_KM_DOT_HUNDREDS_SEG					6
// #define DS_RPM_KM_DOT_DOZENS_PIN						DS_Pin_15
// #define DS_RPM_KM_DOT_DOZENS_SEG						6

#define Version 				0 //ver 0
/* ---- Mask hien thi HUD: moi vung 1 bit rieng, khop 11 checkbox web config ---- */
#define HUD_REGION_SPEED		0x0001  // Toc Do
#define HUD_REGION_TURN			0x0002  // Xi Nhan
#define HUD_REGION_GEAR			0x0004  // Can So
#define HUD_REGION_DRIVEMODE	0x0008  // Che Do Lai
#define HUD_REGION_HANDBRAKE	0x0010  // Phanh Tay
#define HUD_REGION_SEATBELT		0x0020  // Day An Toan
#define HUD_REGION_BATTERY		0x0040  // % Pin
#define HUD_REGION_RANGE		0x0080  // Range
#define HUD_REGION_DOOR			0x0100  // Canh Bao Dong Cua
#define HUD_REGION_HIGHBEAM		0x0200  // Den Xa
#define HUD_REGION_LOWBEAM		0x0400  // Den Gan
/* 3 vung tu bien bao BLE (WYN sign) - khac 8 vung tren o cho chung duoc
 * VE LAI moi khi co event BLE moi (on_wyn_sign trong lib_ble.cpp / vong lap
 * trong main.cpp), khong phai moi vong lap doc lai tu CAN nhu cac vung tren. */
#define HUD_REGION_SPEED_LIMIT	0x0800  // Toc do gioi han (bien bao)
#define HUD_REGION_CAMERA		0x1000  // Camera (toc do/giao thong/AI)
#define HUD_REGION_NO_OVERTAKING	0x2000  // Cam vuot
#define HUD_REGION_MASK_ALL		0x3FFF  // 14 bit = 1


#define HUD_REGION_LIGHT		(HUD_REGION_HIGHBEAM | HUD_REGION_LOWBEAM)
#define HUD_REGION_WARNING		(HUD_REGION_HANDBRAKE | HUD_REGION_SEATBELT | HUD_REGION_DOOR)
#define VF3         			0
#define VF34        			1
#define VF5         			2
#define VF6 					3
#define VF7 					4
#define LIMO 					5
#define MINIO 			  		6
#define ECVAN 					7
#define FADIL 					8
#define LUX 					9
#define LYNKCO1  				10
#define LYNKCO6 				11
#define MAZDA3_2018 			12 // 2015 - 2019
#define MAZDA3_2021 			13 // 2020 - 2025
#define MAZDACX5				14 
#define Ford_XLS_2019			15 // 2018 - 2019
#define Huyndai_Creta   		16 // 2023
#define EX2						17
#define TOTAL_MODEL				18 // Neu update model thi thay doi so nay

typedef struct {
	uint8_t  seg;
	uint16_t pin;
	uint8_t  state;
} DigitMap_t;

extern uint8_t carmodel_count;
extern uint8_t prev_carmodel_count;

extern uint8_t flag_display_setup;
extern bool flag_display_carmodel;
extern bool flag_display_partition;
extern bool Led_EN;
extern bool Led_Turn_Signal;
extern uint16_t display_partition_warning;				
extern uint16_t display_partition_battery;				
extern uint16_t display_partition_time;						
extern uint16_t display_partition_gear;						
extern uint16_t display_partition_range;	
extern uint16_t display_partition_driveMode;	

extern uint16_t display_partition_driveMode_from_flash;
extern uint16_t display_partition_battery_from_flash;
extern uint16_t hud_display_mask;
void HUD_SetDisplayMask(uint16_t mask);
void HUD_SaveDisplayMask(uint16_t mask);
uint16_t HUD_GetDisplayMask(void);


extern const char *const CarModelNames[TOTAL_MODEL];

void LED_Setup(void);
void LED_TestAll(void);
void LED_Full_On(void);
void LED_Off(void);
void LED_Control(void);
void LED_Setup_CarModel(void);

void SM16206_Camera(uint8_t state);
void SM16206_NoPassing(uint8_t state);
void SM16206_Speed_Limit(uint8_t number);

#endif
#ifndef _LIB_CANBUS_H_
#define _LIB_CANBUS_H_

#include "lib_led.h"
#include "stdbool.h"
#include "driver/gpio.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <driver/twai.h>

// ESP32 GPIO Pin Definitions
#define CAN_EN_PIN   GPIO_NUM_8  // CAN Transceiver Enable
#define CAN_RX_PIN   GPIO_NUM_1   // CAN RX (GPIO4)
#define CAN_TX_PIN   GPIO_NUM_0  // CAN TX (GPIO5)

typedef struct {
	uint16_t speed;
	uint16_t range;
	
	uint8_t  time_hour;
	uint8_t  time_minute;
	uint8_t  batery_pecent;
	uint8_t  gear;
	uint8_t  drive_mode;
	
	bool  beam_low;
	bool  beam_high;
	bool  parking_brake;
	bool  tpms;
	bool  seat_belt;
	bool  door_open;
	bool  turn_left_signal;
	bool  turn_right_signal;
	bool 	blind_left;
	bool 	blind_right;
} Car_Can_Infor_t;

extern Car_Can_Infor_t car_can_infor;


///////////////////////// Message Structure for ESP32
typedef struct {
	uint32_t StdId;
	uint8_t Data[8];
	uint8_t data_length_code;
	bool is_extended_id;
} CanMsg_t;

extern CanMsg_t can_rx_current;

///////////////////////// Danh rieng cho xe Lux
typedef struct {
	uint8_t lim_speed;
	uint8_t page_id;
	uint8_t normal_ratio;
	uint8_t sport_ratio;
	uint16_t dac_min;
} Set_Lim_t;

extern Set_Lim_t LUX_Set_Lim;
extern bool Lim_Lux_Model;
/////////////////////////
void CAN1_Setup(void);
void CAN1_Transmit_Message(CanMsg_t can_tx);
void CAN1_RX_Task(void *pvParameters);  // FreeRTOS Task thay cho interrupt handler
void CAN_MessageHandler(CanMsg_t *can_rx);

void CAN_VF3(CanMsg_t *can_rx);
void CAN_VFe34(CanMsg_t *can_rx);
void CAN_VF5(CanMsg_t *can_rx);
void CAN_VF6(CanMsg_t *can_rx);
void CAN_VF7(CanMsg_t *can_rx);
void CAN_LIMO(CanMsg_t *can_rx);
void CAN_LUX(CanMsg_t *can_rx);
void CAN_LYNK_CO_1(CanMsg_t *can_rx);
void CAN_LYNK_CO_6(CanMsg_t *can_rx);
void CAN_FADIL(CanMsg_t *can_rx);
void CAN_ECVAN(CanMsg_t *can_rx);
void CAN_MINIO(CanMsg_t *can_rx);
void CAN_MAZDA3_2018(CanMsg_t *can_rx);
void CAN_MAZDA3_2021(CanMsg_t *can_rx);
void CAN_MAZDACX5(CanMsg_t *can_rx);
void CAN_Ford_XLS_2019(CanMsg_t *can_rx);
void CAN_Huyndai_Creta(CanMsg_t *can_rx);
void CAN_EX2(CanMsg_t *can_rx);

#endif

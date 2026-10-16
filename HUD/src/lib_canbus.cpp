#include "lib_canbus.h"
#include <string.h>

#include "esp_log.h"
#include "web_config.h"

CanMsg_t can_rx_current;

Car_Can_Infor_t car_can_infor;

Set_Lim_t LUX_Set_Lim;
bool Lim_Lux_Model = 0;

// TWAI Queue for CAN messages
static QueueHandle_t can_rx_queue = NULL;
static const int CAN_RX_QUEUE_SIZE = 10;

void CAN1_Setup(void)
{
   can_rx_queue = xQueueCreate(CAN_RX_QUEUE_SIZE, sizeof(twai_message_t));
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)CAN_TX_PIN,
        (gpio_num_t)CAN_RX_PIN,
        TWAI_MODE_NORMAL
    );
    g_config.tx_queue_len  = 5;
    g_config.rx_queue_len  = 5;
    g_config.alerts_enabled = TWAI_ALERT_NONE;
    g_config.clkout_divider = 0;
    g_config.intr_flags    = ESP_INTR_FLAG_LEVEL1;

    twai_timing_config_t t_config;
	memset(&t_config, 0, sizeof(t_config));
t_config.clk_src       = TWAI_CLK_SRC_DEFAULT;  // IDF v5.x thêm field này
t_config.quanta_resolution_hz = 0;               // 0 = dùng default
t_config.brp           = 8;
t_config.tseg_1        = 15;
t_config.tseg_2        = 4;
t_config.sjw           = 3;
t_config.triple_sampling = false;

    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    // if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
    //     twai_start();
    // }
	esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
if (err != ESP_OK) {
    ESP_LOGE("CAN", "Driver install FAILED: %s", esp_err_to_name(err));
} else {
    ESP_LOGI("CAN", "Driver install OK");
    err = twai_start();
    if (err != ESP_OK) ESP_LOGE("CAN", "Start FAILED: %s", esp_err_to_name(err));
    else ESP_LOGI("CAN", "TWAI started OK");
}

    gpio_config_t io_conf;
    memset(&io_conf, 0, sizeof(io_conf));
    io_conf.pin_bit_mask = (1ULL << CAN_EN_PIN);
    io_conf.mode         = GPIO_MODE_OUTPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en   = GPIO_PULLUP_DISABLE;
    io_conf.intr_type    = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level((gpio_num_t)CAN_EN_PIN, 0);

    xTaskCreatePinnedToCore(CAN1_RX_Task, "CAN1_RX_Task", 4096, NULL, 5, NULL, 0);
}

void CAN1_Transmit_Message(CanMsg_t can_tx)
{
    twai_message_t tx_msg;
    tx_msg.identifier = can_tx.StdId;
    tx_msg.extd = can_tx.is_extended_id;
    tx_msg.data_length_code = can_tx.data_length_code;
    memcpy(tx_msg.data, can_tx.Data, can_tx.data_length_code);
    
    // Gửi gói tin ra mạng CAN bus
    twai_transmit(&tx_msg, pdMS_TO_TICKS(10));
}

void CAN1_RX_Task(void *pvParameters)
{   
    twai_message_t message;
    CanMsg_t local_rx_msg; 
    
    extern uint8_t carmodel_count; 
    
    while (1) 
    {  static uint32_t last_log = 0;
uint32_t now = xTaskGetTickCount();
if (now - last_log > pdMS_TO_TICKS(1000)) {
    last_log = now;
    twai_status_info_t status;
    twai_get_status_info(&status);
			}
       
        if (twai_receive(&message, pdMS_TO_TICKS(20)) == ESP_OK) 
        {
            
            local_rx_msg.StdId = message.identifier;
            local_rx_msg.data_length_code = message.data_length_code;
            local_rx_msg.is_extended_id = message.extd;
            
            // Ép giới hạn DLC tối đa là 8 byte để bảo vệ bộ nhớ
            uint8_t dlc = (message.data_length_code > 8) ? 8 : message.data_length_code;
            memcpy(local_rx_msg.Data, message.data, dlc);
            
            // Cập nhật lại vào biến global phục vụ các thư viện khác đọc trạng thái nếu cần
            memcpy(&can_rx_current, &local_rx_msg, sizeof(CanMsg_t));
            
            // Định tuyến cuộc gọi phân tích gói tin dựa trên dòng xe hiện tại
            switch(carmodel_count)
            {
                case VF3:           CAN_VF3(&local_rx_msg);           break;
                case VF34:          CAN_VFe34(&local_rx_msg);         break;
                case VF5:           CAN_VF5(&local_rx_msg);           break;
                case VF6:           CAN_VF6(&local_rx_msg);           break;
                case VF7:           CAN_VF7(&local_rx_msg);           break;
                case LIMO:          CAN_LIMO(&local_rx_msg);          break;
                case LUX:           CAN_LUX(&local_rx_msg);           break;
                case LYNKCO1:       CAN_LYNK_CO_1(&local_rx_msg);     break;
                case LYNKCO6:       CAN_LYNK_CO_6(&local_rx_msg);     break;
                case FADIL:         CAN_FADIL(&local_rx_msg);         break;
                case ECVAN:         CAN_ECVAN(&local_rx_msg);         break;
                case MINIO:         CAN_MINIO(&local_rx_msg);         break;
                case MAZDA3_2018:   CAN_MAZDA3_2018(&local_rx_msg);   break;
                case MAZDA3_2021:   CAN_MAZDA3_2021(&local_rx_msg);   break;
                case MAZDACX5:      CAN_MAZDACX5(&local_rx_msg);      break;
                case Ford_XLS_2019: CAN_Ford_XLS_2019(&local_rx_msg); break;
                case Huyndai_Creta: CAN_Huyndai_Creta(&local_rx_msg); break;
                case EX2:        	CAN_EX2(&local_rx_msg);     	  break;
                default: break;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1)); 
    }
}

void CAN_VF3(CanMsg_t *can_rx){
			/* ACC */
	if(can_rx->StdId == 0x112){
		/////// Không có ACC => LED OFF
		if((((can_rx->Data[4])>>1)&0x01)!=1){
			flag_Reset_RPM = 1;
			Led_EN = 0;
			WiFi_Disable();
		}
		////// Có ACC => LED ON
		else{
			flag_Reset_RPM = 0;
			Led_EN = 1;
			WiFi_Enable();
		}
	}
	/* Vehicle Speed */
	if(can_rx->StdId == 0x2BF)
	{
		car_can_infor.speed = (((can_rx->Data[6] & 0x7F) << 8) | can_rx->Data[7]) * 0.01;
	}
	
	/* Gear Position */
	if(can_rx->StdId == 0x269)
	{
		car_can_infor.gear = (can_rx->Data[1] & 0xE0) >> 5;
	}
	
	/* Range */
	if(can_rx->StdId == 0x2E9)
	{
		car_can_infor.range = ((can_rx->Data[4] << 8) | (can_rx->Data[5])) / 10;
	}
	
	/* Batery Percent */
	if(can_rx->StdId == 0x379)
	{
		car_can_infor.batery_pecent = can_rx->Data[7] & 0x7F;
	}
	
	/* Drive mode */
	if(can_rx->StdId == 0x379)
	{
		car_can_infor.drive_mode = (can_rx->Data[0] & 0x38) >> 3;
	}
	
	/* Time */
	if(can_rx->StdId == 0x43F)
	{
		car_can_infor.time_hour = can_rx->Data[3] & 0x1F;
		car_can_infor.time_minute = can_rx->Data[4] & 0x3F;
	}
	
	/* Turn Signal */
	if(can_rx->StdId == 0x106)
	{
		car_can_infor.beam_low =  can_rx->Data[5] & 0x03;
		car_can_infor.beam_high = (can_rx->Data[3] & 0xC0) >> 6;
		car_can_infor.turn_left_signal = (can_rx->Data[5] & 0x10) >> 4;
		car_can_infor.turn_right_signal = (can_rx->Data[5] & 0x20) >> 5;
	}
	
	/* -------------------- Warnning -------------------- */
	/* TPMS no have */
	
	/* Parking Brake */
	if(can_rx->StdId == 0x102)
	{
		if(((can_rx->Data[2] & 0x20) >> 5) == 1)
		{
			car_can_infor.parking_brake = 0;
		}
		else if(((can_rx->Data[2] & 0x20) >> 5) == 0)
		{
			car_can_infor.parking_brake = 1;
		}
	}
	
	/* Door open */
	if(can_rx->StdId == 0x105)
	{
		car_can_infor.door_open = 0;
		
		/* Pass Door */
		if(((can_rx->Data[2] & 0x30) >> 4) == 1)
		{
			car_can_infor.door_open = 1;
		}
		/* Driver Door */
		if(((can_rx->Data[2] & 0x0C) >> 2) == 1)
		{
			car_can_infor.door_open = 1;
		}
		/* Trunk */
		if((can_rx->Data[2] & 0x03) == 1)
		{
			car_can_infor.door_open = 1;
		}
	}
	
	/* Seat belt */
	if(can_rx->StdId == 0x381)
	{
		car_can_infor.seat_belt = 0;
		
		/* Pass_SeatBelt */
		if((can_rx->Data[2] & 0x03) == 1)
		{
			if((can_rx->Data[3] & 0x03) == 0)
			{
				car_can_infor.seat_belt = 1;
			}
		}
		/* Driver_SeatBelt */
		if(((can_rx->Data[3] & 0x0C) >> 2) == 0)
		{
			car_can_infor.seat_belt = 1;
		}
	}
}

void CAN_VFe34(CanMsg_t *can_rx){
	/* Vehicle Speed */
	if(can_rx->StdId == 0x43F)
	{
		car_can_infor.speed = ((can_rx->Data[6] & 0x7F) << 8 | can_rx->Data[7]) * 0.01;
	}
	
	/* Gear Position */
	if(can_rx->StdId == 0x379)
	{
		car_can_infor.gear = can_rx->Data[3] & 0x07;
	}
	
	/* Range */
	if(can_rx->StdId == 0x3E0)
	{
		car_can_infor.range = ((can_rx->Data[2] << 4) | (can_rx->Data[3] >> 4)) / 2;
	}
	
	/* Batery Percent */
	if(can_rx->StdId == 0x379)
	{
		car_can_infor.batery_pecent = (can_rx->Data[7] & 0x7F);
	}
	
	/* Drive mode */
	if(can_rx->StdId == 0x379)
	{
		car_can_infor.drive_mode = (can_rx->Data[0] & 0x38) >> 3;
	}
	
	/* Time */
	if(can_rx->StdId == 0x43F)
	{
		car_can_infor.time_hour = can_rx->Data[3] & 0x1F;
		car_can_infor.time_minute = can_rx->Data[4] & 0x3F;
	}
	
	/* Turn Signal */
	if(can_rx->StdId == 0x106)
	{
		car_can_infor.beam_low = can_rx->Data[5] & 0x03;
		car_can_infor.beam_high = (can_rx->Data[3] & 0xC0) >> 6;
		
		if ((can_rx->Data[5] & 0x30) >> 4 == 0x01) 
		{
			car_can_infor.turn_left_signal = 1;
			car_can_infor.turn_right_signal = 0;
		}
		else if ((can_rx->Data[5] & 0x30) >> 4 == 0x02) 
		{
			car_can_infor.turn_left_signal = 0;
			car_can_infor.turn_right_signal = 1;
		}
		else if ((can_rx->Data[5] & 0x30) >> 4 == 0x03) 
		{
			car_can_infor.turn_left_signal = 1;
			car_can_infor.turn_right_signal = 1;
		}
		else if ((can_rx->Data[5] & 0x30) >> 4 == 0x00) 
		{
			car_can_infor.turn_left_signal = 0;
			car_can_infor.turn_right_signal = 0;
		}
	}
	
	/* -------------------- Warnning -------------------- */
	/* TPMS no have */
	
	/* Parking Brake */
	if(can_rx->StdId == 0x2BF)
	{
		if((can_rx->Data[3] & 0x30) >> 4 == 2)
		{
			car_can_infor.parking_brake = 1;
		}
		else if((can_rx->Data[3] & 0x30) >> 4 == 0)
		{
			car_can_infor.parking_brake = 0;
		}
	}
	
	/* Door open */
	if(can_rx->StdId == 0x105)
	{
		car_can_infor.door_open = 0;
		
		/* FL door */
		if(((can_rx->Data[2] & 0x0C) >> 2) == 1)
		{
			car_can_infor.door_open = 1;
		}
		
		/* FR door */
		if(((can_rx->Data[2] & 0x30) >> 4) == 1)
		{
			car_can_infor.door_open = 1;
		}
		
		/* RL door */
		if(((can_rx->Data[2] & 0xC0) >> 6) == 1)
		{
			car_can_infor.door_open = 1;
		}
		
		/* RR door */
		if((can_rx->Data[3] & 0x03) == 1)
		{
			car_can_infor.door_open = 1;
		}
		
		/* Trunk */
		if((can_rx->Data[2] & 0x03) == 1)
		{
			car_can_infor.door_open = 1;
		}
		
		/* Capo */
		if(((can_rx->Data[3] & 0x0C) >> 2) == 1)
		{
			car_can_infor.door_open = 1;
		}
	}
	
	/* Seat belt */
	if(can_rx->StdId == 0x381)
	{
		car_can_infor.seat_belt = 0;
		
		/* Pass_SeatBelt */
		if((can_rx->Data[2] & 0x03) == 2)
		{
			if((can_rx->Data[3] & 0x03) == 0)
			{
				car_can_infor.seat_belt = 1;
			}
		}
		/* Driver_SeatBelt */
		if(((can_rx->Data[3] & 0x0C) >> 2) == 0)
		{
			car_can_infor.seat_belt = 1;
		}
	}
}

void CAN_VF5(CanMsg_t *can_rx){
	/* ACC */
	if(can_rx->StdId == 0x112){
		/////// Không có ACC => LED OFF
		if(can_rx->Data[4] == 0){
			flag_Reset_RPM = 1;
			Led_EN = 0;
			WiFi_Disable();
		}
		////// Có ACC => LED ON
		else{
			flag_Reset_RPM = 0;
			Led_EN = 1;
			WiFi_Enable();
		}
	}
	/* Vehicle Speed */
	if(can_rx->StdId == 0x2BF)
	{
		car_can_infor.speed = ((can_rx->Data[6] & 0x7F) << 8 | can_rx->Data[7]) * 0.01;
	}
	
	/* Gear Position */
	if(can_rx->StdId == 0x379)
	{
		car_can_infor.gear = can_rx->Data[3] & 0x07;
	}
	
	/* Range */
	if(can_rx->StdId == 0x3E0)
	{
		car_can_infor.range = ((can_rx->Data[2] << 4) | (can_rx->Data[3] >> 4)) / 2;
	}
	
	/* Batery Percent */
	if(can_rx->StdId == 0x379)
	{
		car_can_infor.batery_pecent = (can_rx->Data[7] & 0x7F);
	}
	
	/* Drive mode */
	if(can_rx->StdId == 0x379)
	{
		uint8_t raw_data = (can_rx->Data[0] & 0x38) >> 3;
		if(raw_data != 3) car_can_infor.drive_mode = raw_data;
	}
	
	/* Time */
	if(can_rx->StdId == 0x43F)
	{
		car_can_infor.time_hour = can_rx->Data[3] & 0x1F;
		car_can_infor.time_minute = can_rx->Data[4] & 0x3F;
	}
	
	/* Turn Signal */
	if(can_rx->StdId == 0x106)
	{
		if((can_rx->Data[1]&0x30)>>4 == 0x01) 
		{
			car_can_infor.blind_left = 1;
		}
		if((can_rx->Data[1]&0x30)>>4 == 0x02)
		{
			car_can_infor.blind_right = 1;
		}
		if((can_rx->Data[1]&0x30)>>4 == 0x03)
		{
			car_can_infor.blind_left = 1;
			car_can_infor.blind_right = 1;
		}
		if((can_rx->Data[1]&0x30)>>4 == 0x00)
		{
			car_can_infor.blind_left = 0;
			car_can_infor.blind_right = 0;	
		}

		car_can_infor.beam_low = can_rx->Data[5] & 0x03;
		car_can_infor.beam_high = (can_rx->Data[3] & 0xC0) >> 6;
		
		if ((can_rx->Data[5] & 0x30) >> 4 == 0x01) 
		{
			car_can_infor.turn_left_signal = 1;
			car_can_infor.turn_right_signal = 0;
		}
		else if ((can_rx->Data[5] & 0x30) >> 4 == 0x02) 
		{
			car_can_infor.turn_left_signal = 0;
			car_can_infor.turn_right_signal = 1;
		}
		else if ((can_rx->Data[5] & 0x30) >> 4 == 0x03) 
		{
			car_can_infor.turn_left_signal = 1;
			car_can_infor.turn_right_signal = 1;
		}
		else if ((can_rx->Data[5] & 0x30) >> 4 == 0x00) 
		{
			car_can_infor.turn_left_signal = 0;
			car_can_infor.turn_right_signal = 0;
		}
	}
	
	/* -------------------- Warnning -------------------- */
	/* TPMS */
	if(can_rx->StdId == 0x482)
	{
		car_can_infor.tpms = 0;
		
		if(((can_rx->Data[0] & 0x80) >> 7) == 1)
		{
			car_can_infor.tpms = 1;
		}
		
		if(((can_rx->Data[0] & 0x02) >> 1) == 1)
		{
			car_can_infor.tpms = 1;
		}
		
		if(((can_rx->Data[1] & 0x80) >> 7) == 1)
		{
			car_can_infor.tpms = 1;
		}
		
		if(((can_rx->Data[1] & 0x02) >> 1) == 1)
		{
			car_can_infor.tpms = 1;
		}
	}
	
	/* Parking Brake */
	if(can_rx->StdId == 0x2BF)
	{
		car_can_infor.parking_brake = (can_rx->Data[3] & 0x30) >> 4;
	}
	
	/* Door open */
	if(can_rx->StdId == 0x105)
	{
		car_can_infor.door_open = 0;
		
		/* FL door */
		if(((can_rx->Data[2] & 0x0C) >> 2) == 1)
		{
			car_can_infor.door_open = 1;
		}
		
		/* FR door */
		if(((can_rx->Data[2] & 0x30) >> 4) == 1)
		{
			car_can_infor.door_open = 1;
		}
		
		/* RL door */
		if(((can_rx->Data[2] & 0xC0) >> 6) == 1)
		{
			car_can_infor.door_open = 1;
		}
		
		/* RR door */
		if((can_rx->Data[3] & 0x03) == 1)
		{
			car_can_infor.door_open = 1;
		}
		
		/* Trunk */
		if((can_rx->Data[2] & 0x03) == 1)
		{
			car_can_infor.door_open = 1;
		}
		
		/* Capo */
		if(((can_rx->Data[3] & 0x0C) >> 2) == 1)
		{
			car_can_infor.door_open = 1;
		}
	}
	
	/* Seat belt */
	if(can_rx->StdId == 0x381)
	{
		car_can_infor.seat_belt = 0;
		
		/* Pass_SeatBelt */
		if((can_rx->Data[2] & 0x03) == 1)
		{
			if((can_rx->Data[3] & 0x03) == 0)
			{
				car_can_infor.seat_belt = 1;
			}
		}
		/* Driver_SeatBelt */
		if(((can_rx->Data[3] & 0x0C) >> 2) == 0)
		{
			car_can_infor.seat_belt = 1;
		}
	}
}

void CAN_VF6(CanMsg_t *can_rx){
	/* Control LED*/
	if(can_rx->StdId == 0x107){
		if(car_can_infor.gear == 0){ /// Gear = P
			/// Lock => LED OFF
			if(can_rx->Data[2] == 0x40){
				flag_Reset_RPM = 1;
				Led_EN = 0;
				WiFi_Disable();
			}
			/// Unlock => LED ON
			else{
				flag_Reset_RPM = 0;
				Led_EN = 1;
				WiFi_Enable();
			}
		}
	}
	/* Vehicle Speed */
	if(can_rx->StdId == 0x40D)
	{
		car_can_infor.speed = can_rx->Data[2] << 1 | (can_rx->Data[3] & 0x80) >> 7;
	}
	
	/* Gear Position */
	if(can_rx->StdId == 0x0D9)
	{
		car_can_infor.gear = can_rx->Data[4] & 0x07;
	}
	
	/* Range */
	if(can_rx->StdId == 0x3E0)
	{
		car_can_infor.range = ((can_rx->Data[2] << 4) | (can_rx->Data[3] >> 4)) / 2;
	}
	
	/* Batery Percent */
	if(can_rx->StdId == 0x43A)
	{
		car_can_infor.batery_pecent = (((can_rx->Data[5] & 0x7F ) << 3) | ((can_rx->Data[6] & 0xE0) >> 5)) * 0.1;
	}
	
	/* Drive mode */
	if(can_rx->StdId == 0xD9)
	{
		uint8_t raw_data = (can_rx->Data[4] & 0x70) >> 4;
		if(raw_data!= 3)
		car_can_infor.drive_mode = raw_data;
	}
	
	/* Time */
	if(can_rx->StdId == 0x2F8)
	{
		car_can_infor.time_hour = can_rx->Data[0];
		car_can_infor.time_minute = can_rx->Data[1];
	}
	
	/* Turn Signal */
	if(can_rx->StdId == 0x106)
	{
		car_can_infor.beam_low = can_rx->Data[5] & 0x01;
		car_can_infor.beam_high = (can_rx->Data[3] & 0xC0) >> 6;
		car_can_infor.turn_left_signal = (can_rx->Data[6] & 0x40) >> 6;
		car_can_infor.turn_right_signal = (can_rx->Data[6] & 0x80) >> 7;
	}
	
	/* -------------------- Warnning -------------------- */
	/* Blind spot */
	if(can_rx->StdId == 0x132)
	{
		car_can_infor.blind_left  = (can_rx->Data[3]) & 0x01;
    car_can_infor.blind_right = (can_rx->Data[3] >> 2) & 0x01;
	}

	/* TPMS */
	if(can_rx->StdId == 0x483)
	{
		car_can_infor.tpms = 0;
		
		/* FL_W */
		if((can_rx->Data[4] & 0x07) == 2)
		{
			car_can_infor.tpms = 1;
		}
		
		/* FR_W */
		if(((can_rx->Data[4] & 0x70) >> 4) == 2)
		{
			car_can_infor.tpms = 1;
		}
		
		/* RL_W */
		if((can_rx->Data[5] & 0x07) == 2)
		{
			car_can_infor.tpms = 1;
		}
		
		/* RR_W */
		if(((can_rx->Data[5] & 0x70) >> 4) == 2)
		{
			car_can_infor.tpms = 1;
		}
	}
	
	/* Parking Brake */
	if(can_rx->StdId == 0x4A7)
	{
		car_can_infor.parking_brake = (can_rx->Data[2] & 0x20) >> 5;
	}
	
	/* Door open */
	if(can_rx->StdId == 0x105)
	{
		car_can_infor.door_open = 0;
		
		/* FL door */
		if(((can_rx->Data[2] & 0x0C) >> 2) == 1)
		{
			car_can_infor.door_open = 1;
		}
		
		/* FR door */
		if(((can_rx->Data[2] & 0x30) >> 4) == 1)
		{
			car_can_infor.door_open = 1;
		}
		
		/* RL door */
		if(((can_rx->Data[2] & 0xC0) >> 6) == 1)
		{
			car_can_infor.door_open = 1;
		}
		
		/* RR door */
		if((can_rx->Data[3] & 0x03) == 1)
		{
			car_can_infor.door_open = 1;
		}
		
		/* Trunk */
		if((can_rx->Data[2] & 0x03) == 1)
		{
			car_can_infor.door_open = 1;
		}
		
		/* Capo */
		if(((can_rx->Data[3] & 0x04) >> 3) == 1)
		{
			car_can_infor.door_open = 1;
		}
	}
	
	/* Seat belt */
	if(can_rx->StdId == 0x381)
	{
		car_can_infor.seat_belt = 0;
		
		/* Pass_SeatBelt */
		if((can_rx->Data[2] & 0x03) == 1)
		{
			if((can_rx->Data[3] & 0x03) == 0)
			{
				car_can_infor.seat_belt = 1;
			}
		}
		/* Driver_SeatBelt */
		if(((can_rx->Data[3] & 0x0C) >> 2) == 0)
		{
			car_can_infor.seat_belt = 1;
		}
	}
	/* Blind spot */
	if(can_rx->StdId == 0x132)
	{
		car_can_infor.blind_left  = (can_rx->Data[3]) & 0x01;
    car_can_infor.blind_right = (can_rx->Data[3] >> 2) & 0x01;
	}
}

void CAN_VF7(CanMsg_t *can_rx){
	/* Control LED*/
	if(can_rx->StdId == 0x107){
		if(car_can_infor.gear == 0){ /// Gear = P
			/// Lock => LED OFF
			if(can_rx->Data[2] == 0x40){
				flag_Reset_RPM = 1;
				Led_EN = 0;
				WiFi_Disable();
			}
			/// Unlock => LED ON
			else{
				flag_Reset_RPM = 0;
				Led_EN = 1;
				WiFi_Enable();
			}
		}
	}
	/* Vehicle Speed */
	if(can_rx->StdId == 0x40D)
	{
		car_can_infor.speed = can_rx->Data[2] << 1 | (can_rx->Data[3] & 0x80) >> 7;
	}
	
	/* Gear Position */
	if(can_rx->StdId == 0x0D9)
	{
		car_can_infor.gear = can_rx->Data[4] & 0x07;
	}
	
	/* Range */
	if(can_rx->StdId == 0x3E0)
	{
		car_can_infor.range = ((can_rx->Data[2] << 4) | (can_rx->Data[3] >> 4)) / 2;
	}
	
	/* Batery Percent */
	if(can_rx->StdId == 0x43A)
	{
		car_can_infor.batery_pecent = (((can_rx->Data[5] & 0x7F ) << 3) | ((can_rx->Data[6] & 0xE0) >> 5)) * 0.1;
	}
	
	/* Drive mode */
	if(can_rx->StdId == 0xD9)
	{
		uint8_t raw_data = (can_rx->Data[4] & 0x70) >> 4;
		if(raw_data!= 3)
		car_can_infor.drive_mode = raw_data;
	}
	
	/* Time */
	if(can_rx->StdId == 0x2F8)
	{
		car_can_infor.time_hour = can_rx->Data[0];
		car_can_infor.time_minute = can_rx->Data[1];
	}
	
	/* Turn Signal */
	if(can_rx->StdId == 0x106)
	{
		car_can_infor.beam_low = can_rx->Data[5] & 0x01;
		car_can_infor.beam_high = (can_rx->Data[3] & 0xC0) >> 6;
		car_can_infor.turn_left_signal = (can_rx->Data[6] & 0x40) >> 6;
		car_can_infor.turn_right_signal = (can_rx->Data[6] & 0x80) >> 7;
	}
	
	/* -------------------- Warnning -------------------- */
	/* TPMS */
	if(can_rx->StdId == 0x483)
	{
		car_can_infor.tpms = 0;
		
		/* FL_W */
		if((can_rx->Data[4] & 0x07) == 2)
		{
			car_can_infor.tpms = 1;
		}
		
		/* FR_W */
		if(((can_rx->Data[4] & 0x70) >> 4) == 2)
		{
			car_can_infor.tpms = 1;
		}
		
		/* RL_W */
		if((can_rx->Data[5] & 0x07) == 2)
		{
			car_can_infor.tpms = 1;
		}
		
		/* RR_W */
		if(((can_rx->Data[5] & 0x70) >> 4) == 2)
		{
			car_can_infor.tpms = 1;
		}
	}
	
	/* Parking Brake */
	if(can_rx->StdId == 0x4A7)
	{
		car_can_infor.parking_brake = (can_rx->Data[2] & 0x20) >> 5;
	}
	
	/* Door open */
	if(can_rx->StdId == 0x105)
	{
		car_can_infor.door_open = 0;
		
		/* FL door */
		if(((can_rx->Data[2] & 0x0C) >> 2) == 1)
		{
			car_can_infor.door_open = 1;
		}
		
		/* FR door */
		if(((can_rx->Data[2] & 0x30) >> 4) == 1)
		{
			car_can_infor.door_open = 1;
		}
		
		/* RL door */
		if(((can_rx->Data[2] & 0xC0) >> 6) == 1)
		{
			car_can_infor.door_open = 1;
		}
		
		/* RR door */
		if((can_rx->Data[3] & 0x03) == 1)
		{
			car_can_infor.door_open = 1;
		}
		
		/* Trunk */
		if((can_rx->Data[2] & 0x03) == 1)
		{
			car_can_infor.door_open = 1;
		}
		
		/* Capo */
		if(((can_rx->Data[3] & 0x04) >> 3) == 1)
		{
			car_can_infor.door_open = 1;
		}
	}
	
	/* Seat belt */
	if(can_rx->StdId == 0x381)
	{
		car_can_infor.seat_belt = 0;
		
		/* Pass_SeatBelt */
		if((can_rx->Data[2] & 0x03) == 1)
		{
			if((can_rx->Data[3] & 0x03) == 0)
			{
				car_can_infor.seat_belt = 1;
			}
		}
		/* Driver_SeatBelt */
		if(((can_rx->Data[3] & 0x0C) >> 2) == 0)
		{
			car_can_infor.seat_belt = 1;
		}
	}
	/* Blind spot */
	if(can_rx->StdId == 0x132)
	{
		car_can_infor.blind_left  = (can_rx->Data[3]) & 0x01;
    car_can_infor.blind_right = (can_rx->Data[3] >> 2) & 0x01;
	}
}

void CAN_LIMO(CanMsg_t *can_rx){
	static bool lock = false;
	static bool unlock = false;
	static bool BCM_Lock_Door = false;
	/* Control LED*/
		/// Da khoa cua va co tin hieu Lock => LED OFF
	if(BCM_Lock_Door && lock){
		flag_Reset_RPM = 1;
		Led_EN = 0;
		WiFi_Disable();
	}
	  /// Co tin hieu unlock => LED ON
	else if(unlock){
	   flag_Reset_RPM = 0;
	   Led_EN = 1;
	   WiFi_Enable();
	}
	/* Lock & Unlock */
	if(can_rx->StdId == 0x2F1){
		lock = (can_rx->Data[0] >> 4) & 0x01;
		unlock = (can_rx->Data[0] >> 6) & 0x01;
	}
	/* Vehicle Speed */
	if(can_rx->StdId == 0x40D)
	{
		car_can_infor.speed = ((can_rx->Data[2] << 1) | ((can_rx->Data[3] >> 7) & 0x01));
	}
	
	/* Gear Position */
	if(can_rx->StdId == 0x162)
	{
		car_can_infor.parking_brake = 0;
		
		if(((can_rx->Data[2] >> 2) & 0x07) == 0x01) // P
		{
			car_can_infor.gear = 0;
			car_can_infor.parking_brake = 1;
		}
		else if(((can_rx->Data[2] >> 2) & 0x07) == 0x02) // R
		{
			car_can_infor.gear = 1;
		}
		else if(((can_rx->Data[2] >> 2) & 0x07) == 0x03) // N
		{
			car_can_infor.gear = 2;
		}
		else if(((can_rx->Data[2] >> 2) & 0x07) == 0x04) // D
		{
			car_can_infor.gear = 3;
		}
	}
	
	/* Range */
	if(can_rx->StdId == 0x2E9)
	{
		car_can_infor.range = (((uint32_t)can_rx->Data[2] << 24) | ((uint32_t)can_rx->Data[3] << 16) | ((uint32_t)can_rx->Data[4] << 8)  |  ((uint32_t)can_rx->Data[5])) * 0.1;
	}
	
	/* Batery Percent */
	if(can_rx->StdId == 0x37D)
	{
		car_can_infor.batery_pecent = ((can_rx->Data[2] << 2) | ((can_rx->Data[3] >> 6) & 0x03)) * 0.1;
	}
	
	/* Drive mode */
	if(can_rx->StdId == 0x166)
	{
		if((can_rx->Data[2] & 0x07) == 0x01) // Normal mode
		{
			car_can_infor.drive_mode = 0;
		}
		else if((can_rx->Data[2] & 0x07) == 0x02) // Eco mode
		{
			car_can_infor.drive_mode = 1;
		}
		else if((can_rx->Data[2] & 0x07) == 0x03) // Sport mode
		{
			car_can_infor.drive_mode = 2;
		}
	}
	
	/* Time */
	if(can_rx->StdId == 0x29E)
	{
		car_can_infor.time_hour = can_rx->Data[2] & 0x1F;
		car_can_infor.time_minute = can_rx->Data[1] & 0x3F;
	}
	
	/* Light Signal */
	if(can_rx->StdId == 0x106)
	{
		car_can_infor.turn_left_signal = 0;
		car_can_infor.turn_right_signal = 0;
		
		car_can_infor.beam_low = (can_rx->Data[2] >> 4) & 0x03;
		car_can_infor.beam_high = (can_rx->Data[2] >> 6) & 0x03;
		
		if((can_rx->Data[2] & 0x03) == 0x01) // Turn left
		{
			car_can_infor.turn_left_signal = 1;
		}
		else if((can_rx->Data[2] & 0x03) == 0x02) // Turn right
		{
			car_can_infor.turn_right_signal = 1;
		}
		else if((can_rx->Data[2] & 0x03) == 0x03) // Turn left & right
		{
			car_can_infor.turn_left_signal = 1;
			car_can_infor.turn_right_signal = 1;
		}
	}
	
	/* Door open */
	if(can_rx->StdId == 0x4D4)
	{
		car_can_infor.door_open = 0;
		
		/* FL door */
		if(((can_rx->Data[0] >> 2) & 0x03) == 0x01)
		{
			car_can_infor.door_open = 1;
		}
		
		/* FR door */
		if(((can_rx->Data[0] >> 4) & 0x03) == 0x01)
		{
			car_can_infor.door_open = 1;
		}
		
		/* RL door */
		if(((can_rx->Data[0] >> 6) & 0x03) == 0x01)
		{
			car_can_infor.door_open = 1;
		}
		
		/* RR door */
		if((can_rx->Data[3] & 0x03) == 0x01)
		{
			car_can_infor.door_open = 1;
		}
		
		/* Trunk */
		if(((can_rx->Data[0] >> 1) & 0x01) == 0x01)
		{
			car_can_infor.door_open = 1;
		}
		
		/* Capo */
		if((can_rx->Data[0] & 0x01) == 0x01)
		{
			car_can_infor.door_open = 1;
		}
		
		/* BCM_LOCK_DOOR */
		if(((can_rx->Data[2] >> 6) & 0x03) == 0){
			BCM_Lock_Door = 0;
		}
		else BCM_Lock_Door = 1;
	}
	
	/* Seat belt */
	if(can_rx->StdId == 0x372)
	{
		car_can_infor.seat_belt = 0;
		
		if(((can_rx->Data[1] >> 4) & 0x01) == 0x01)
		{
			car_can_infor.seat_belt = 1;
		}
	}
}

void CAN_LUX(CanMsg_t *can_rx){
	static uint32_t engine_rpm_sum = 0;
	static uint32_t engine_rpm = 0;
	static uint8_t engine_rpm_sum_count = 0;
	
	static uint16_t speed_average = 0;
	static uint8_t sum_speed = 0;
	/* Rpm */
	if(can_rx->StdId == 0x0A5)
	{
		engine_rpm_sum += (can_rx->Data[6] * 256 + can_rx->Data[5]) / 4;
    engine_rpm_sum_count++;
    if (engine_rpm_sum_count >= 10) {
      engine_rpm = engine_rpm_sum / 10;
      engine_rpm_sum_count = 0;
      engine_rpm_sum = 0;
			flag_Reset_RPM =(engine_rpm != 0)? 0:1;
			if(flag_Reset_RPM == 0) WiFi_Enable();
			else WiFi_Disable();
    }
	}
	/* Vehicle Speed */
	if(can_rx->StdId == 0x1A1)
	{
		speed_average += (can_rx->Data[3] * (5.0f/1.18)) + (can_rx->Data[2] * (5.0f/257));
		sum_speed++;
		if(sum_speed == 10) {
			car_can_infor.speed = speed_average/10.0f;
			sum_speed = 0;
			speed_average = 0;
		}
	}
	
	/* Range */
	if(can_rx->StdId == 0x330)
	{
		car_can_infor.range = ((uint16_t)(can_rx->Data[7] << 8) | (uint16_t)can_rx->Data[6])/16.0f;
	}
	
	/* Gear Position */
	if(can_rx->StdId == 0x215)
	{
		switch((can_rx->Data[7] >> 1) & 0x07){
			case 1: car_can_infor.gear = 4; break; //M
			case 3: car_can_infor.gear = 3; break; //D
			case 4: car_can_infor.gear = 2; break; //N
			case 5: car_can_infor.gear = 0; break; //P
			case 6: car_can_infor.gear = 1; break; //R
			default: break;
		}
	}
	
	/* Time */
	if(can_rx->StdId == 0x283)
	{
		car_can_infor.time_hour = can_rx->Data[2] & 0x1F;
		car_can_infor.time_minute = can_rx->Data[1] & 0x3F;
	}
	
	/* Light Signal */
	if(can_rx->StdId == 0x106)
	{
		car_can_infor.beam_low = (can_rx->Data[1] >> 2) & 0x01;
		car_can_infor.beam_high = (can_rx->Data[0] >> 4) & 0x01;
		car_can_infor.turn_left_signal = (can_rx->Data[2] >> 4) & 0x01; // Turn left
		car_can_infor.turn_right_signal = (can_rx->Data[2] >> 5) & 0x01; // Turn right
	}
	
	/* Door open */
	if(can_rx->StdId == 0x105)
	{
		car_can_infor.door_open = ((can_rx->Data[0] >> 2) & 0x01)
														 |((can_rx->Data[0] >> 4) & 0x01)
														 |((can_rx->Data[0] >> 6) & 0x01)
														 |((can_rx->Data[1] >> 2) & 0x01)
														 |(can_rx->Data[1] & 0x01)
														 |(can_rx->Data[0] & 0x01);
	}
	/* Seat belt */
	if(can_rx->StdId == 0x381)
	{
		car_can_infor.seat_belt = 0;
		if(!(can_rx->Data[1] >> 4) & 0x01){
			car_can_infor.seat_belt = 1;
		}
		if(((can_rx->Data[2] >> 6) & 0x01) && !((can_rx->Data[1] >> 6) & 0x01)){
			car_can_infor.seat_belt = 1;
		}											  
	}
	/* Brake */
	if(can_rx->StdId == 0x4A9)
	{
		car_can_infor.parking_brake = !((can_rx->Data[1] >> 6) & 0x01);
	}
	/* LIM */
	if(can_rx->StdId == 0x100)
	{	
		/* Hien thi them thong tin xe lap LIM*/
		Lim_Lux_Model = 1;
		COUNT_TIME_UART_LIM_LUX = 0;
		/* Driver Mode */
		if((can_rx->Data[1] & 0x01) == 0) // Normal mode
		{
			car_can_infor.drive_mode = 0;																																
		}
		else if((can_rx->Data[1] & 0x01) == 1) // Sport mode
		{
			car_can_infor.drive_mode = 2;
		}
		/* Page ID */
		LUX_Set_Lim.page_id = can_rx->Data[0] & 0x0F;
		/* normal_ratio  */
		LUX_Set_Lim.normal_ratio = can_rx->Data[2];
		/* sport_ratio ID */
		LUX_Set_Lim.sport_ratio = can_rx->Data[3];
		/* dac_min */
		LUX_Set_Lim.dac_min = ((uint16_t)can_rx->Data[4] << 8) | can_rx->Data[5];
		/* lim_speed */
		LUX_Set_Lim.lim_speed = can_rx->Data[6];
	}
}

void CAN_LYNK_CO_6(CanMsg_t *can_rx){
	static bool door_open_raw_t = 0;
  static bool door_open_raw_s = 0;
	/* Rpm */
	if(can_rx->StdId == 0x085)
	{
			flag_Reset_RPM =(can_rx->Data[2] != 0)? 0:1;
			if(flag_Reset_RPM == 0) WiFi_Enable();
			else WiFi_Disable();
	}
	/* Vehicle Speed */
	if(can_rx->StdId == 0x125)
	{
		car_can_infor.speed = can_rx->Data[1] * 2 +  can_rx->Data[2] / 100;
		if (car_can_infor.speed > 40) 
		{
			car_can_infor.speed = car_can_infor.speed - 2;
		}
		
		if (car_can_infor.speed > 60) 
		{
			car_can_infor.speed = car_can_infor.speed - 2;
		}
		
		if (car_can_infor.speed > 80) 
		{
			car_can_infor.speed = car_can_infor.speed - 2;
		}
		
		if (car_can_infor.speed > 100) 
		{
			car_can_infor.speed = car_can_infor.speed - 2;
		}
		
		if (car_can_infor.speed > 120) 
		{
			car_can_infor.speed = car_can_infor.speed - 2;
		}
	}
	
	/* Gear Position */
	if(can_rx->StdId == 0x113)
	{
		if (can_rx->Data[2] == 0x00 && can_rx->Data[3] == 0x00 && can_rx->Data[4] == 0xAA && can_rx->Data[5] == 0x09)
		{
			car_can_infor.gear = 0; // P
		}
		else if (can_rx->Data[2] == 0x00 && can_rx->Data[3] == 0x04 && can_rx->Data[4] == 0xBB && can_rx->Data[5] == 0x12)
		{
			car_can_infor.gear = 1; // R
		}
		else if (can_rx->Data[2] == 0x00 && can_rx->Data[3] == 0x00 && can_rx->Data[4] == 0x00 && can_rx->Data[5] == 0x1B)
		{
			car_can_infor.gear = 2; // N
		}
		else if (can_rx->Data[2] == 0x00 && can_rx->Data[3] == 0x01 && can_rx->Data[4] == 0x11 && can_rx->Data[5] == 0x24)
		{
			car_can_infor.gear = 3; // D
		}
		else if (can_rx->Data[2] == 0x00 && can_rx->Data[3] == 0x01 && can_rx->Data[4] == 0x11 && can_rx->Data[5] == 0x2D)
		{
		  car_can_infor.gear = 4; // M1
		}
		else if (can_rx->Data[2] == 0x00 && can_rx->Data[3] == 0x04 && can_rx->Data[4] == 0x22 && can_rx->Data[5] == 0x35)
		{
			car_can_infor.gear = 4; // M2
		}
	}
		
	/* Drive mode */
	if(can_rx->StdId == 0x2FD)
	{
		if(can_rx->Data[4] == 0x14) // Intelligent mode
		{
			car_can_infor.drive_mode = 0;
		}
		else if(can_rx->Data[4] == 0x04) // Eco mode
		{
			car_can_infor.drive_mode = 1;
		}
		else if(can_rx->Data[4] == 0x08) // Sport mode
		{
			car_can_infor.drive_mode = 2;
		}
		else if(can_rx->Data[4] == 0x00) // Cof mode
		{
			car_can_infor.drive_mode = 3;
		}
	}
	
	/* Time */
	if(can_rx->StdId == 0x3F0)
	{
		car_can_infor.time_hour = can_rx->Data[2];
		car_can_infor.time_minute = can_rx->Data[1];
	}
	
	/* Light Signal */
	if(can_rx->StdId == 0x1F0)
	{
		car_can_infor.turn_left_signal = 0;
		car_can_infor.turn_right_signal = 0;
		car_can_infor.beam_low = can_rx->Data[1] & 0x01;
		car_can_infor.beam_high = (can_rx->Data[1] >> 2) & 0x01;
		
		if(((can_rx->Data[0] >> 0) & 0x01) == 0x01) // Turn left
		{
			car_can_infor.turn_left_signal = 1;
		}
		
		if(((can_rx->Data[0] >> 3) & 0x01) == 0x01) // Turn right
		{
			car_can_infor.turn_right_signal = 1;
		}
	}
	
	/* -------------------- Warnning -------------------- */
	/* TPMS */
	
	/* Parking Brake */
	if(can_rx->StdId == 0x125)
	{
		car_can_infor.parking_brake = 0;
		
		if(can_rx->Data[3] == 0x08)
		{
			car_can_infor.parking_brake = 1;
		}
	}
	
	/* Door open */
	if(can_rx->StdId == 0x285)
	{
		car_can_infor.door_open = 0;
		door_open_raw_t = 0;
		/* FL door */
		if(can_rx->Data[4] == 0x01)
		{
			door_open_raw_t = 1;
		}
		/* RL door */
		else if(can_rx->Data[4] == 0x02)
		{
			door_open_raw_t = 1;
		}
	}
	
	if(can_rx->StdId == 0x286)
	{
		car_can_infor.door_open = 0;
		door_open_raw_s = 0;
		/* FR door */
		if(can_rx->Data[4] == 0x01)
		{
			door_open_raw_s = 1;
		}
		/* RR door */
		else if(can_rx->Data[4] == 0x02)
		{
			door_open_raw_s = 1;
		}
	}
	if(door_open_raw_t || door_open_raw_s)
	{
		car_can_infor.door_open = 1;
	}
	/* Seat belt */
	if(can_rx->StdId == 0x380)
	{
		car_can_infor.seat_belt = 0;
		
		if(((can_rx->Data[0] >> 0) & 0x01) == 0x01)
		{
			car_can_infor.seat_belt = 1;
		}
		
		if(((can_rx->Data[0] >> 2) & 0x01) == 0x01)
		{
			if(((can_rx->Data[0] >> 1) & 0x01) == 0x01)
			{
				car_can_infor.seat_belt = 1;
			}
		}
	}
}

void CAN_FADIL(CanMsg_t *can_rx){
	static uint16_t speed_left = 0;
	static uint16_t speed_right = 0;
	static uint16_t speed_average = 0;
	static uint8_t sum = 0;
	/* Rpm */
	if(can_rx->StdId == 0x0C9)
	{
			flag_Reset_RPM =(can_rx->Data[1] != 0)? 0:1;
			if(flag_Reset_RPM == 0) WiFi_Enable();
			else WiFi_Disable();
	}
	
	/* Vehicle Speed */
	if(can_rx->StdId == 0x348){
		if(can_rx->Data[0] == 0) speed_left = (can_rx->Data[1]*7)/235;
		else speed_left = can_rx->Data[0]*8.3 + can_rx->Data[1]/20;
		if(can_rx->Data[2] == 0) speed_right = (can_rx->Data[3]*7)/235;
		else speed_right = can_rx->Data[2]*8.3 + can_rx->Data[3]/20;
		speed_average += (speed_left + speed_right)/2;
		sum++;
		if(sum == 10) {
			car_can_infor.speed = speed_average/10.0f;
			sum = 0;
			speed_average = 0;
		}
	}
	
	/* Gear Position */
	if(can_rx->StdId == 0x1F5)
	{
		switch(can_rx->Data[3] & 0x07){
			case 1: car_can_infor.gear = 0; break; //P
			case 2: car_can_infor.gear = 1; break; //R
			case 3: car_can_infor.gear = 2; break; //N
			case 4: car_can_infor.gear = 3; break; //D
			case 5: car_can_infor.gear = 4; break; //L
			default: break;
		}
	}
	
	/* Brake */
	if(can_rx->StdId == 0x1F1)
	{
		car_can_infor.parking_brake = ((can_rx->Data[4] >> 4) & 0x01);
	}
	
	/* Light Signal */
	if(can_rx->StdId == 0x140)
	{
		car_can_infor.beam_low =((can_rx->Data[1] >> 3) & (can_rx->Data[1] >> 4)) & 0x01;
		car_can_infor.beam_high = (can_rx->Data[0] >> 7) & 0x01;
		car_can_infor.turn_left_signal = (can_rx->Data[2] >> 2) & 0x01; 
		car_can_infor.turn_right_signal = (can_rx->Data[2] >> 3) & 0x01; 
	}
	
	/* Door & Seat belt */
	if(can_rx->StdId == 0x12A)
	{
		car_can_infor.door_open = ((can_rx->Data[1] >> 1) & 0x01)
														 |((can_rx->Data[1] >> 2) & 0x01)
														 |((can_rx->Data[2] >> 7) & 0x01)
														 |(can_rx->Data[1] & 0x01);
		car_can_infor.seat_belt = !((can_rx->Data[1] >> 4) & 0x01);
	}
}

void CAN_ECVAN(CanMsg_t *can_rx){	
	/* ACC ON */
	if(can_rx->StdId == 0x112)
	{
		if(((can_rx->Data[4] & 0x0F) == 0x0B) || ((can_rx->Data[4] & 0x0F) == 0x0A)) {
			flag_Reset_RPM = 0;
			Led_EN = 1;
			WiFi_Enable();
		}
		else {
			flag_Reset_RPM = 1;
			Led_EN = 0;
			WiFi_Disable();
		}
	}
	/* Vehicle Speed */
	if(can_rx->StdId == 0x2BF)
	{
		car_can_infor.speed = (((can_rx->Data[6] & 0x7F) << 8) | can_rx->Data[7]) * 0.01;
	}
	
	/* Gear Position */
	if(can_rx->StdId == 0x269)
	{
		switch((can_rx->Data[1] & 0xE0) >> 5){
			case 1: car_can_infor.gear = 1; break; //R
			case 2: car_can_infor.gear = 2; break; //N
			case 3: car_can_infor.gear = 3; break; //D
			default: break;
		}
	}
	
	/* Range */
	if(can_rx->StdId == 0x2E9)
	{
		car_can_infor.range = ((can_rx->Data[4] << 8) | (can_rx->Data[5]))/10.0f;
	}
	
	/* Batery Percent */
	if(can_rx->StdId == 0x215)
	{
		car_can_infor.batery_pecent = (((can_rx->Data[3] & 0x03) << 8) | can_rx->Data[4])/10.0f;
	}
	
	/* Turn Signal */
	if(can_rx->StdId == 0x106)
	{
		car_can_infor.beam_low = can_rx->Data[5] & 0x01;
		car_can_infor.beam_high = (can_rx->Data[3] >> 6) & 0x01;
		car_can_infor.turn_left_signal = (can_rx->Data[5] >> 4) & 0x01;
		car_can_infor.turn_right_signal = (can_rx->Data[5] >> 5) & 0x01;
	}
	
	/* -------------------- Warnning -------------------- */	
	/* Parking Brake */
	if(can_rx->StdId == 0x2BF)
	{
		car_can_infor.parking_brake = !((can_rx->Data[2] >>5) & 0x01);
	}
	
	/* Door open */
	if(can_rx->StdId == 0x105)
	{
		car_can_infor.door_open = 0;
		if((can_rx->Data[2]>>2) & 0x01)
		{
			car_can_infor.door_open = 1;
		}
		if((can_rx->Data[2]>>4) & 0x01)
		{
			car_can_infor.door_open = 1;
		}	
		if((can_rx->Data[3]>>6) & 0x01)
		{
			car_can_infor.door_open = 1;
		}	
		/* Seat belt */
		car_can_infor.seat_belt = 0;
		if(!(((can_rx->Data[5]>>2) & 0x01)))
		{
			car_can_infor.seat_belt = 1;
		}
		if(((can_rx->Data[4]>>3) & 0x01) && (!(can_rx->Data[5] & 0x01)))
		{
			car_can_infor.seat_belt = 1;
		}
	}
}

void CAN_MINIO(CanMsg_t *can_rx){	
			/* ACC ON */
	if(can_rx->StdId == 0x201)
	{
		if((can_rx->Data[6] & 0x03) == 1) {
			flag_Reset_RPM = 0;
			Led_EN = 1;
			WiFi_Enable();
		}
		else {
			flag_Reset_RPM = 1;
			Led_EN = 0;
			WiFi_Disable();
		}
	}
	/* Vehicle Speed */

	if(can_rx->StdId == 0x2BF)
	{
		car_can_infor.speed = (((can_rx->Data[6] & 0x7F) << 8) | can_rx->Data[7]) * 0.01;
	}
	
	if(can_rx->StdId == 0x155) // oke
	{
		if(((can_rx->Data[4] >> 1) & 0x07) == 0x00) // P
		{
			car_can_infor.gear = 0;
		}
		else if(((can_rx->Data[4] >> 1) & 0x07) == 0x01) // R
		{
			car_can_infor.gear = 1;
		}
		else if(((can_rx->Data[4] >> 1) & 0x07) == 0x02) // N
		{
			car_can_infor.gear = 2;
		}
		else if(((can_rx->Data[4] >> 1) & 0x07) == 0x03) // D
		{
			car_can_infor.gear = 3;
		}
	}
	
	/* Brake */ 
	if(can_rx->StdId == 0x4C5){
		car_can_infor.parking_brake = 0;
		if(((can_rx->Data[2] >> 4) & 0x01))
			car_can_infor.parking_brake = 1;
	}
		
	
	/* Range */  
	if(can_rx->StdId == 0x120)
	{
		car_can_infor.range = (((can_rx->Data[6] << 8) | (can_rx->Data[7])) - 40) / 5 ;
		if(((((can_rx->Data[6] << 8) | (can_rx->Data[7])) - 40) % 5 ) > 2 ){
			car_can_infor.range += 1;
		}
	}
	
	/* Batery Percent */  
	
	if(can_rx->StdId == 0x214)  // oke
	{
		car_can_infor.batery_pecent = ((can_rx->Data[2]<<4 ) | ((can_rx->Data[3]) >>4)) * 0.1  ;
		if((((can_rx->Data[2]<<4 ) | ((can_rx->Data[3]) >>4))% 10 ) > 7 ) 
		{
				car_can_infor.batery_pecent = 	car_can_infor.batery_pecent + 1;
		}
	}
	
	/* Drive mode */
	if(can_rx->StdId == 0x4C5) 
	{
		if(((can_rx->Data[7] >> 2) & 0x03) == 0x00) // Normal mode
		{
			car_can_infor.drive_mode = 0;
		}
		else if(((can_rx->Data[7] >> 2) & 0x03) == 0x01) // Eco mode
		{
			car_can_infor.drive_mode = 1;
		}
		else if(((can_rx->Data[7] >> 2) & 0x03) == 0x02) // Sport mode
		{
			car_can_infor.drive_mode = 2;
		}
	}
	
	/* Time */
	if(can_rx->StdId == 0x43F) // check
	{
		car_can_infor.time_hour = (can_rx->Data[3] + 9 )% 24  ;
		car_can_infor.time_minute = (can_rx->Data[4]+ 5) % 60; 
	}
	
	/* Light Signal */
	if(can_rx->StdId == 0x106) //oke
	{
		car_can_infor.turn_left_signal = 0;
		car_can_infor.turn_right_signal = 0;
		
		car_can_infor.beam_low = (can_rx->Data[2] >> 4) & 0x03;
		car_can_infor.beam_high = (can_rx->Data[2] >> 6) & 0x03;
		
		if((can_rx->Data[2] & 0x03) == 0x01) // Turn left
		{
			car_can_infor.turn_left_signal = 1;
		}
		else if((can_rx->Data[2] & 0x03) == 0x02) // Turn right
		{
			car_can_infor.turn_right_signal = 1;
		}
		else if((can_rx->Data[2] & 0x03) == 0x03) // Turn left & right
		{
			car_can_infor.turn_left_signal = 1;
			car_can_infor.turn_right_signal = 1;
		}
	}
	
	
	/* Door open */
	if(can_rx->StdId == 0x4D4) // oke
	{
		car_can_infor.door_open = 0;
		
		/* FL door */
		if(((can_rx->Data[0]) & 0x03) == 0x01)
		{
			car_can_infor.door_open = 1;
		}
		
		/* FR door */
		if(((can_rx->Data[0] >> 2) & 0x03) == 0x01)
		{
			car_can_infor.door_open = 1;
		}
	}
	
	/* Seat belt */
	if(can_rx->StdId == 0x488) // check
	{
		car_can_infor.seat_belt = 0;
		
		if(((can_rx->Data[5] ) & 0x01) == 0x01)
		{
			car_can_infor.seat_belt = 1;
		}
	}
}
void CAN_LYNK_CO_1(CanMsg_t *can_rx){
//	static bool door_open_raw_t = 0;
//  static bool door_open_raw_s = 0;
	
	static uint16_t speed_average = 0;
	static uint8_t sum_speed = 0;
	/* Rpm */
	if(can_rx->StdId == 0x66)
	{
			flag_Reset_RPM =(can_rx->Data[3] != 0)? 0:1;
			if(flag_Reset_RPM == 0) WiFi_Enable();
			else WiFi_Disable();
	}
	/* Vehicle Speed */
	if(can_rx->StdId == 0x181)
	{
		speed_average += ((uint16_t)(can_rx->Data[4] << 8) | can_rx->Data[5])/92.0f;
		sum_speed++;
		if(sum_speed >= 10) {
			car_can_infor.speed = speed_average/10.0f;
			sum_speed = 0;
			speed_average = 0;
		}
	}
	
	/* Gear Position */
	if(can_rx->StdId == 0x185)
	{
		switch((can_rx->Data[6] >> 4) & 0x0F){
			case 0x08: car_can_infor.gear = 0; break;
			case 0x0A: car_can_infor.gear = 1; break;
			case 0x0C: car_can_infor.gear = 2; break;
			case 0x0E: car_can_infor.gear = 3; break;
			default:													 break;
		}
	}
		
	/* Drive mode */
	if(can_rx->StdId == 0x155)
	{
		switch(can_rx->Data[0] & 0x0C){
			case 0x0C: car_can_infor.drive_mode = 1; break; //ECO 
			case 0x04: car_can_infor.drive_mode = 2; break; //SPORT
			case 0x00: car_can_infor.drive_mode = 3; break; //COM
			default:													 break;
		}
	}
	
	/* Time */
//	if(can_rx->StdId == 0x3F0)
//	{
//		car_can_infor.time_hour = can_rx->Data[2];
//		car_can_infor.time_minute = can_rx->Data[1];
//	}
	
	/* Light Signal */
//	if(can_rx->StdId == 0x1F0)
//	{
//		car_can_infor.turn_left_signal = 0;
//		car_can_infor.turn_right_signal = 0;
//		car_can_infor.beam_low = can_rx->Data[1] & 0x01;
//		car_can_infor.beam_high = (can_rx->Data[1] >> 2) & 0x01;
//		
//		if(((can_rx->Data[0] >> 0) & 0x01) == 0x01) // Turn left
//		{
//			car_can_infor.turn_left_signal = 1;
//		}
//		
//		if(((can_rx->Data[0] >> 3) & 0x01) == 0x01) // Turn right
//		{
//			car_can_infor.turn_right_signal = 1;
//		}
//	}
	
	/* -------------------- Warnning -------------------- */
	/* TPMS */
	
	/* Parking Brake */
	if(can_rx->StdId == 0x158)
	{
		car_can_infor.parking_brake = !((can_rx->Data[1] >> 7) & 0x01);
	}
	
	/* Door open */
//	if(can_rx->StdId == 0x285)
//	{
//		car_can_infor.door_open = 0;
//		door_open_raw_t = 0;
//		/* FL door */
//		if(can_rx->Data[4] == 0x01)
//		{
//			door_open_raw_t = 1;
//		}
//		/* RL door */
//		else if(can_rx->Data[4] == 0x02)
//		{
//			door_open_raw_t = 1;
//		}
//	}
//	
//	if(can_rx->StdId == 0x286)
//	{
//		car_can_infor.door_open = 0;
//		door_open_raw_s = 0;
//		/* FR door */
//		if(can_rx->Data[4] == 0x01)
//		{
//			door_open_raw_s = 1;
//		}
//		/* RR door */
//		else if(can_rx->Data[4] == 0x02)
//		{
//			door_open_raw_s = 1;
//		}
//	}
//	if(door_open_raw_t || door_open_raw_s)
//	{
//		car_can_infor.door_open = 1;
//	}
	/* Seat belt */
//	if(can_rx->StdId == 0x380)
//	{
//		car_can_infor.seat_belt = 0;
//		
//		if(((can_rx->Data[0] >> 0) & 0x01) == 0x01)
//		{
//			car_can_infor.seat_belt = 1;
//		}
//		
//		if(((can_rx->Data[0] >> 2) & 0x01) == 0x01)
//		{
//			if(((can_rx->Data[0] >> 1) & 0x01) == 0x01)
//			{
//				car_can_infor.seat_belt = 1;
//			}
//		}
//	}
}

void CAN_MAZDA3_2018(CanMsg_t *can_rx){
	/* Vehicle Speed */
	static uint16_t speed_average = 0;
	static uint8_t sum_speed = 0;
	if (can_rx->StdId == 0x202)
	{
		flag_Reset_RPM = 0;
		uint16_t raw = ((uint16_t)can_rx->Data[2] << 8) | can_rx->Data[3];
		uint16_t speed = (raw * 280)/256/100;  
		speed_average += speed;
		sum_speed++;
		if (sum_speed >= 10)
		{
				car_can_infor.speed = speed_average / 10;  
				sum_speed = 0;
				speed_average = 0;
		}
	}
	else flag_Reset_RPM = 1;
	
	/* Gear Position */
	if(can_rx->StdId == 0x131)
	{
		uint8_t gear = can_rx->Data[1] & 0x07;
		if(gear == 1) car_can_infor.gear = 0;
		else if(gear == 2) car_can_infor.gear = 1;
		else if(gear == 3) car_can_infor.gear = 2;
		else if(gear == 4) car_can_infor.gear = 3;
		if((can_rx->Data[2] >> 7) & 0x01) car_can_infor.gear = 4;
	}
		
	/* Drive mode */
	if(can_rx->StdId == 0x09E)
	{
		if((can_rx->Data[7] >> 4) & 0x01) car_can_infor.drive_mode = 2;
		else car_can_infor.drive_mode = 0;
	}
	
	/* Turn Signal */
	if(can_rx->StdId == 0x91)
	{
		if (((can_rx->Data[0] >> 4) & 0x0F) == 0x0F){
			car_can_infor.beam_low = 1;
		}
		else car_can_infor.beam_low = 0;
		
		if(((can_rx->Data[0] >> 3) & 0x01) || ((can_rx->Data[1] >> 7) & 0x01))
		{
			car_can_infor.beam_high = 1;
		}
		else car_can_infor.beam_high = 0;
	}
	if(can_rx->StdId == 0x09A){
		car_can_infor.turn_left_signal = (can_rx->Data[2] >> 2) & 0x01;
		car_can_infor.turn_right_signal = (can_rx->Data[2] >> 3) & 0x01;
	}
	
	/* -------------------- Warnning -------------------- */
	
	/* Parking Brake */
	if(can_rx->StdId == 0x203)
	{
		car_can_infor.parking_brake = (can_rx->Data[0] >> 6) & 0x01;
	}
	
	/* Door open */
	if(can_rx->StdId == 0x43E)
	{
		car_can_infor.door_open = 0;
		
		/* FL door */
		if(((can_rx->Data[4] >> 4) & 0x01) == 1)
		{
			car_can_infor.door_open = 1;
		}
		
		/* FR door */
		if(((can_rx->Data[4] >> 5) & 0x01) == 1)
		{
			car_can_infor.door_open = 1;
		}
		
		/* RL door */
		if(((can_rx->Data[4] >> 3) & 0x01) == 1)
		{
			car_can_infor.door_open = 1;
		}
		
		/* RR door */
		if(((can_rx->Data[4] >> 2) & 0x01) == 1)
		{
			car_can_infor.door_open = 1;
		}
		
		/* Trunk */
		if(((can_rx->Data[5] >> 7) & 0x01) == 1)
		{
			car_can_infor.door_open = 1;
		}
		
		/* Capo */
		if(((can_rx->Data[4] >> 1) & 0x01) == 1)
		{
			car_can_infor.door_open = 1;
		}
	}
	
	/* Seat belt */
	if(can_rx->StdId == 0x340)
	{
		car_can_infor.seat_belt = 0;
		
		/* Pass_SeatBelt */
		if(((can_rx->Data[3] >> 5) & 0x01) == 1)
		{
			if(((can_rx->Data[3] >> 2) & 0x01) == 0)
			{
				car_can_infor.seat_belt = 1;
			}
		}
		/* Driver_SeatBelt */
		if(((can_rx->Data[3] >> 3) & 0x01) == 0)
		{
			car_can_infor.seat_belt = 1;
		}
	}
}

void CAN_MAZDA3_2021(CanMsg_t *can_rx){
	/* Vehicle Speed */
	static uint16_t speed_average = 0;
	static uint8_t sum_speed = 0;
	if (can_rx->StdId == 0x217)
	{
		uint16_t speed;
		uint16_t raw = ((uint16_t)can_rx->Data[4] << 8) | can_rx->Data[5];
		if(raw <= 1000) speed = raw/90.0f;  
		else if(raw > 1000 && raw <= 2000) speed = raw/92.0f;  
		else if(raw > 2000 && raw <= 4000) speed = raw/94.0f;  
		else speed = raw/96.0f;  
		speed_average += speed;
		sum_speed++;
		if (sum_speed >= 20)
		{
				car_can_infor.speed = speed_average / 20;  
				sum_speed = 0;
				speed_average = 0;
		}
	}
	
	/* Gear Position */
	if(can_rx->StdId == 0x228)
	{
		uint8_t gear = can_rx->Data[0] & 0x07;
		if(gear == 1) car_can_infor.gear = 0;
		else if(gear == 2) car_can_infor.gear = 1;
		else if(gear == 3) car_can_infor.gear = 2;
		else if(gear == 4) car_can_infor.gear = 3;
		else if(gear == 6) car_can_infor.gear = 4;
	}
		
	/* Drive mode */
	if(can_rx->StdId == 0x09F)
	{
		if((can_rx->Data[2] & 0x03) == 3) car_can_infor.drive_mode = 2; //Sport
		else car_can_infor.drive_mode = 0; //Normal
	}
	
	/* Turn Signal */
	if(can_rx->StdId == 0x91)
	{
		car_can_infor.beam_low = (can_rx->Data[0] >> 4) & 0x01;
		car_can_infor.beam_high = ((can_rx->Data[0] >> 3) & 0x01)|((can_rx->Data[1] >> 7) & 0x01);
	}
	if(can_rx->StdId == 0x43F){
		car_can_infor.turn_left_signal = can_rx->Data[4] & 0x01;
		car_can_infor.turn_right_signal = (can_rx->Data[4] >> 2) & 0x01;
	}
	
	/* -------------------- Warnning -------------------- */
	
	/* Parking Brake */
	if(can_rx->StdId == 0x203)
	{
		car_can_infor.parking_brake = (can_rx->Data[3] == 0x50)? 1:0;
	}
	
	/* Door open */
	if(can_rx->StdId == 0x09F)
	{
		car_can_infor.door_open = 0;
		if((can_rx->Data[6] >> 6) & 0x01) car_can_infor.door_open = 1;
		if(!((can_rx->Data[6] >> 7) & 0x01)) car_can_infor.door_open = 1;
	}
	
	/* Seat belt */
	static bool driver_belt = 0;
	static bool pass_belt = 0;
	if(can_rx->StdId == 0x99)
	{
		driver_belt = ((can_rx->Data[6] >> 4) & 0x01)? 0:1;
	}
	if(can_rx->StdId == 0x340)
	{
		if((can_rx->Data[3] >> 5) & 0x01){
			pass_belt = ((can_rx->Data[4] >> 6) & 0x01)? 1:0;	
		}
		else pass_belt = 1;
	}
	car_can_infor.seat_belt = (driver_belt && pass_belt)? 0:1;	
}

void CAN_MAZDACX5(CanMsg_t *can_rx){
	/* Vehicle Speed */
	static uint16_t speed_average = 0;
	static uint8_t sum_speed = 0;
	if (can_rx->StdId == 0x202)
	{
		flag_Reset_RPM = 0;
		uint16_t speed;
		uint16_t raw = ((uint16_t)can_rx->Data[2] << 8) | can_rx->Data[3];
		speed = raw/100;  
		speed_average += speed;
		sum_speed++;
		if (sum_speed >= 10)
		{
				car_can_infor.speed = speed_average / 10;  
				sum_speed = 0;
				speed_average = 0;
		}
	}
	else flag_Reset_RPM = 1;
	
	/* Gear Position */
	if(can_rx->StdId == 0x131)
	{
		uint8_t gear = can_rx->Data[1] & 0x07;
		if(gear == 1) car_can_infor.gear = 0;
		else if(gear == 2) car_can_infor.gear = 1;
		else if(gear == 3) car_can_infor.gear = 2;
		else if(gear == 4) car_can_infor.gear = 3;
		if((can_rx->Data[2] >> 7) & 0x01) car_can_infor.gear = 4;
	}
		
	/* Drive mode */
	if(can_rx->StdId == 0x09E)
	{
		if((can_rx->Data[7] >> 4) & 0x01) car_can_infor.drive_mode = 2;
		else car_can_infor.drive_mode = 0;
	}
	
	/* Turn Signal */
	if(can_rx->StdId == 0x91)
	{
		if (((can_rx->Data[0] >> 4) & 0x0F) == 0x0F){
			car_can_infor.beam_low = 1;
		}
		else car_can_infor.beam_low = 0;
		
		if(((can_rx->Data[0] >> 3) & 0x01) || ((can_rx->Data[1] >> 7) & 0x01))
		{
			car_can_infor.beam_high = 1;
		}
		else car_can_infor.beam_high = 0;
	}
	if(can_rx->StdId == 0x09A){
		car_can_infor.turn_left_signal = (can_rx->Data[2] >> 2) & 0x01;
		car_can_infor.turn_right_signal = (can_rx->Data[2] >> 3) & 0x01;
	}
	
	/* -------------------- Warnning -------------------- */
	
	/* Parking Brake */
	if(can_rx->StdId == 0x79)
	{
		car_can_infor.parking_brake = (can_rx->Data[3] >> 5) & 0x01;
	}
	
	/* Door open */
	if(can_rx->StdId == 0x43E)
	{
		car_can_infor.door_open = 0;
		
		/* FL door */
		if(((can_rx->Data[4] >> 5) & 0x01) == 1)
		{
			car_can_infor.door_open = 1;
		}
		
		/* FR door */
		if(((can_rx->Data[4] >> 4) & 0x01) == 1)
		{
			car_can_infor.door_open = 1;
		}
		
		/* RL door */
		if(((can_rx->Data[4] >> 3) & 0x01) == 1)
		{
			car_can_infor.door_open = 1;
		}
		
		/* RR door */
		if(((can_rx->Data[4] >> 2) & 0x01) == 1)
		{
			car_can_infor.door_open = 1;
		}
		
		/* Trunk */
		if((can_rx->Data[4] & 0x01) == 1)
		{
			car_can_infor.door_open = 1;
		}
		
		/* Capo */
		if(((can_rx->Data[4] >> 1) & 0x01) == 1)
		{
			car_can_infor.door_open = 1;
		}
	}
	
	/* Seat belt */
	if(can_rx->StdId == 0x340)
	{
		car_can_infor.seat_belt = 0;
		
		/* Pass_SeatBelt */
		if(((can_rx->Data[3] >> 5) & 0x01) == 1)
		{
			if(((can_rx->Data[3] >> 2) & 0x01) == 0)
			{
				car_can_infor.seat_belt = 1;
			}
		}
		/* Driver_SeatBelt */
		if(((can_rx->Data[3] >> 3) & 0x01) == 0)
		{
			car_can_infor.seat_belt = 1;
		}
	}
}

void CAN_Ford_XLS_2019(CanMsg_t *can_rx){
	/* Vehicle Speed */
	static uint16_t speed_average = 0;
	static uint8_t sum_speed = 0;
	if (can_rx->StdId == 0x77)
	{
		/* RPM */
		if(can_rx->Data[5] == 0x0F) flag_Reset_RPM = 1;
		else flag_Reset_RPM = 0;
		if(flag_Reset_RPM == 0) WiFi_Enable();
		else WiFi_Disable();
		/* Vehicle Speed */
		uint16_t speed;
		uint16_t raw = ((uint16_t)can_rx->Data[0] << 8) | can_rx->Data[1];
		if(raw < 1300) speed = (raw * 310)/256/100;  
		else if((raw >= 1300) && (raw < 4100)) speed = (raw * 280)/256/100;
    else speed = (raw * 270)/256/100;
		speed_average += speed;
		sum_speed++;
		if (sum_speed >= 10)
		{
				car_can_infor.speed = speed_average / 10;  
				sum_speed = 0;
				speed_average = 0;
		}
	}
	
	/* Gear Position */
	if(can_rx->StdId == 0x230)
	{
		uint8_t gear = can_rx->Data[1] & 0x0F;
		if(gear == 0) car_can_infor.gear = 0;
		else if(gear == 2) car_can_infor.gear = 1;
		else if(gear == 4) car_can_infor.gear = 2;
		else if(gear == 6) car_can_infor.gear = 3;
		else if(gear == 8) car_can_infor.gear = 4;
	}
			
	/* Light and Door */
	if(can_rx->StdId == 0x3B3)
	{
		/* Turn signal */
		car_can_infor.turn_left_signal = (can_rx->Data[6] >> 6) & 0x01;
		car_can_infor.turn_right_signal = (can_rx->Data[4] >> 3) & 0x01;
		/* Parking Brake */
		car_can_infor.parking_brake = (can_rx->Data[6] >> 7) & 0x01;
		/* Door open */
		car_can_infor.door_open = ((can_rx->Data[7] >> 5) & 0x01)	// FL
														 |((can_rx->Data[7] >> 4) & 0x01)	// FR
														 |(can_rx->Data[6] & 0x01)				// RL
														 |((can_rx->Data[6] >> 1) & 0x01);// RR
	}
	/* Beam */
	if(can_rx->StdId == 0x3C3){
		car_can_infor.beam_low = (can_rx->Data[0] >> 4) & 0x01;
		car_can_infor.beam_high = (can_rx->Data[0] >> 1) & 0x01;
	}
	/* Seat belt */
	if(can_rx->StdId == 0x04C){
		car_can_infor.seat_belt = 0;
		/* Pass_SeatBelt */
		if((can_rx->Data[0] >> 4) & 0x01) // Co nguoi ngoi
		{
			if(((can_rx->Data[1] >> 4) & 0x01) == 0)
			{
				car_can_infor.seat_belt = 1;
			}
		}
		/* Driver_SeatBelt */
		if(((can_rx->Data[1] >> 6) & 0x01) == 0)
		{
			car_can_infor.seat_belt = 1;
		}
	}
}

void CAN_Huyndai_Creta(CanMsg_t *can_rx){
		/* ACC */
	if(can_rx->StdId == 0x453){
		/////// Không có ACC => LED OFF
		if((can_rx->Data[1]) == 0){
			flag_Reset_RPM = 1;
			Led_EN = 0;
			WiFi_Disable();
		}
		////// Có ACC => LED ON
		else{
			flag_Reset_RPM = 0;
			Led_EN = 1;
			WiFi_Enable();
		}
	}
	/* Vehicle Speed */
	static uint16_t speed_average = 0;
	static uint8_t sum_speed = 0;
	if (can_rx->StdId == 0x368)
	{
		/* Vehicle Speed */
		uint16_t speed;
		uint16_t raw = ((uint16_t)can_rx->Data[1] << 8) | can_rx->Data[2];
		speed = raw/250.0f;
		speed_average += speed;
		sum_speed++;
		if (sum_speed >= 10)
		{
				car_can_infor.speed = speed_average / 10;  
				sum_speed = 0;
				speed_average = 0;
		}
	}
	
	/* Gear Position */
	if(can_rx->StdId == 0x367)
	{
		uint8_t gear = can_rx->Data[4] & 0x0F;
		if(gear == 0) car_can_infor.gear = 0;
		else if(gear == 7) car_can_infor.gear = 1;
		else if(gear == 6) car_can_infor.gear = 2;
		else if(gear == 5) car_can_infor.gear = 3;
		else if(gear == 4 || gear == 8) car_can_infor.gear = 4;
	}
			
	/* Light and Door */
	static bool flag1 = false; // door_541
	static bool flag2 = false; // door_553
	if(can_rx->StdId == 0x541)
	{
		/* Turn signal */
		car_can_infor.turn_left_signal = (can_rx->Data[2] >> 3) & 0x01;
		car_can_infor.turn_right_signal = (can_rx->Data[7] >> 6) & 0x01;
		
		/* Door open */
		flag1 = ((can_rx->Data[1] ) & 0x01)	// 
						|((can_rx->Data[4] >> 3) & 0x01)	// 
		 				|((can_rx->Data[2] >> 1) & 0x01)				// 
   					|((can_rx->Data[1] >> 4) & 0x01);//
	}
	if(can_rx->StdId == 0x553)
	{		
		/* Door open */
		flag2 =(can_rx->Data[3] & 0x01)				// 
					 |((can_rx->Data[2] >> 7) & 0x01);// 
	}
	if (flag1 || flag2) car_can_infor.door_open = 1;
	else car_can_infor.door_open = 0;
	/* Parking Brake */
	if(can_rx->StdId == 0x490){
		car_can_infor.parking_brake = (can_rx->Data[0] ) & 0x01;
	}
	/* Beam */
	if(can_rx->StdId == 0x541){
		car_can_infor.beam_low = (((can_rx->Data[4] >> 5) & 0x07) == 0x04);
		car_can_infor.beam_high = (can_rx->Data[4] & 0x01);
	}
	/* Seat belt */
	if(can_rx->StdId == 1227 ){
		car_can_infor.seat_belt = 0;
		if(((can_rx->Data[0] >> 3) & 0x01))
		{
			car_can_infor.seat_belt = 1;
		}
}
}

void CAN_EX2(CanMsg_t *can_rx){
	
	/* ACC */
	if(can_rx->StdId == 0x2F7){
		/////// Không có ACC => LED OFF
		if((((can_rx->Data[0])>>5)&0x01)==1){
			flag_Reset_RPM = 1;
			Led_EN = 0;
			WiFi_Disable();;
		}
		////// Có ACC => LED ON
		else{
			flag_Reset_RPM = 0;
			Led_EN = 1;
			WiFi_Enable();
		}
	}
	
#define DELAY_SAMPLES  20   
/* Vehicle Speed */
static float speed_buffer[DELAY_SAMPLES] = {0};
static uint8_t buf_index = 0;
static uint8_t buf_filled = 0;
static float speed_raw_display = 0;      // giá tr? float chua làm tròn, dùng d? so sánh hysteresis
static uint16_t speed_shown = 0;         

if (can_rx->StdId == 0x125)
{
    uint16_t raw = ((uint16_t)can_rx->Data[1] << 8) | can_rx->Data[2];
    float speed = raw / 137.6f + 0.3f;

    float delayed_speed = speed_buffer[buf_index];
    speed_buffer[buf_index] = speed;
    buf_index = (buf_index + 1) % DELAY_SAMPLES;

    if (!buf_filled)
    {
        delayed_speed = speed;
        if (buf_index == 0) buf_filled = 1;
    }

    speed_raw_display = delayed_speed;

    // Hysteresis: ch? d?i s? hi?n th? khi l?ch d? xa (>0.6) so v?i m?c dang hi?n th?
    if (speed_raw_display > speed_shown + 0.6f)
    {
        speed_shown = (uint16_t)(speed_raw_display + 0.5f);
    }
    else if (speed_raw_display < speed_shown - 0.6f)
    {
        speed_shown = (uint16_t)(speed_raw_display + 0.5f);
    }

    car_can_infor.speed = speed_shown;
}
	/* Gear Position */
	if(can_rx->StdId == 0x165)
	{
		uint8_t gear = can_rx->Data[1] & 0x0F;
		if(gear == 1) car_can_infor.gear = 0;
		else if(gear == 2) car_can_infor.gear = 1;
		else if(gear == 3) car_can_infor.gear = 2;
		else if(gear == 4) car_can_infor.gear = 3;
	}
	
	/* Drive mode */
	if(can_rx->StdId == 0x283)
	{
		if(can_rx->Data[2] == 0x01) // Eco mode
		{
			car_can_infor.drive_mode = 1;
		}
		else if(can_rx->Data[2] == 0x02) // Sport mode
		{
			car_can_infor.drive_mode = 2;
		}
		else if(can_rx->Data[2] == 0x00) // Com mode
		{
			car_can_infor.drive_mode = 3;
		}
	}
			
	/* Light  */
	if(can_rx->StdId == 0x1F0)
	{
		/* Turn signal + Beam */
		car_can_infor.turn_left_signal = (can_rx->Data[0] & 0x01);
		car_can_infor.turn_right_signal = (can_rx->Data[0] >> 3) & 0x01;
		car_can_infor.beam_low = (((can_rx->Data[1]) & 0x01));
		car_can_infor.beam_high = (can_rx->Data[0] >> 6) & 0x01;
	}
	/* Door open */
	if(can_rx->StdId == 0x285)
	{
		car_can_infor.door_open = 0;
		
		/* FL door */
		if((can_rx->Data[4] & 0x01) == 1)
		{
			car_can_infor.door_open = 1;
		}
		
		/* FR door */
		if(((can_rx->Data[4] >> 1) & 0x01) == 1)
		{
			car_can_infor.door_open = 1;
		}
		
		/* Both 
		if(((can_rx->Data[4] >> 2) & 0x01) == 1)
		{
			car_can_infor.door_open = 1;
		}*/
		}
	if(can_rx->StdId == 0x286){	
		/* FR door */
		if(((can_rx->Data[4] ) & 0x01) == 1)
		{
			car_can_infor.door_open = 1;
		}
		
		/* RR door */
		if(((can_rx->Data[4] >> 1) & 0x01) == 1)
		{
			car_can_infor.door_open = 1;
		}
		
		/* Both 
		
		if(((can_rx->Data[4] >> 2) & 0x01) == 1)
		{
			car_can_infor.door_open = 1;
		}*/
	}
	
	/* Parking Brake */
	if(can_rx->StdId == 0x125){
		car_can_infor.parking_brake = ((can_rx->Data[3])>>3 & 0x01);
	}
	
	/* Seat belt */
	if(can_rx->StdId == 0x380 ){
		car_can_infor.seat_belt = 0;
		if((can_rx->Data[0] & 0x01))
		{
			car_can_infor.seat_belt = 1;
		}
}
}
#include "lib_delay.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

volatile bool flag_blink_led = 0;
volatile bool flag_update_pwm = 0;
volatile bool flag_switch_partition = 0;
volatile bool flag_sample_data = 0;
volatile bool flag_GEAR_M = 0;
volatile bool flag_BLINK_WARNING = 0;
volatile bool flag_Reset_RPM = 0;
volatile bool flag_Reset_Turn = 0;

// Sửa lỗi: Đổi thành kiểu uint32_t để tính toán mượt mà
volatile uint32_t count_clock = 0;

static uint8_t  count_time_pwm = 0;
static uint16_t count_time_blink = 0;
static uint16_t count_time_switch_partition = 0;
static uint16_t count_time_sample_data = 0;
static uint16_t count_time_GEAR_M = 0;
static uint16_t count_time_BLINK_WARNING = 0;
static uint32_t COUNT_TIME_WAIT_RPM = 0;
static uint32_t COUNT_TIME_TURN = 0;

uint32_t COUNT_TIME_UART_LIM_LUX = 0;

static esp_timer_handle_t timer_handle = NULL;

void timer_callback(void* arg)
{
    // Giảm đếm ngược (đã sửa hết lỗi build)
    if (count_clock > 0) {
        count_clock = count_clock - 1;
    }
    
    // Quét nút bấm giống hệt SysTick bản gốc STM32
    BUTTON_ReadAll();

    count_time_blink++;
    if(count_time_blink >= TIME_BLINK_LED)
    {
        count_time_blink = 0;
        flag_blink_led ^= 1;
    }
    
    count_time_pwm++;
    if(count_time_pwm >= TIME_UPDATE_PWM)
    {
        count_time_pwm = 0;
        flag_update_pwm = 1;
    }
    
    count_time_switch_partition++;
    if(count_time_switch_partition >= TIME_SWITCH_PARTITION)
    {
        count_time_switch_partition = 0;
        flag_switch_partition = 1;
    }
    
    count_time_sample_data++;
    if(count_time_sample_data >= TIME_SAMPLE_DATA)
    {
        count_time_sample_data = 0;
        flag_sample_data = 1;
    }
    
    count_time_GEAR_M++;
    if(count_time_GEAR_M >= TIME_GEAR_M)
    {
        count_time_GEAR_M = 0;
        flag_GEAR_M ^= 1;
    }
    
    count_time_BLINK_WARNING++;
    if(count_time_BLINK_WARNING >= TIME_BLINK_WARNING)
    {
        count_time_BLINK_WARNING = 0;
        flag_BLINK_WARNING ^= 1;
    }
    
    if(flag_Reset_Turn) COUNT_TIME_TURN++;
    else {
        COUNT_TIME_TURN = 0;
        Led_Turn_Signal = 1;
    }
    if(COUNT_TIME_TURN >= TIME_TURN)
    {
        COUNT_TIME_TURN = 0;   
        Led_Turn_Signal ^= 1;
    }
    
    if(flag_Reset_RPM) COUNT_TIME_WAIT_RPM++;
    else {
        COUNT_TIME_WAIT_RPM = 0;
        Led_EN = 1;
    }
    if(COUNT_TIME_WAIT_RPM >= TIME_WAIT_RPM)
    {
        COUNT_TIME_WAIT_RPM = 0;    
        Led_EN = 0;
    }
}

void Delay_Setup(void)
{
    // Đã sửa: Đóng gói cấu trúc args chuẩn xác và đầy đủ các trường
    esp_timer_create_args_t timer_args = {
        .callback = &timer_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "periodic_timer",
        .skip_unhandled_events = false
    };

    if (esp_timer_create(&timer_args, &timer_handle) == ESP_OK) {
        esp_timer_start_periodic(timer_handle, 1000); 
    }
}

void Delay_ms(uint16_t time_delay)
{
    count_clock = time_delay;
    while(count_clock > 0)
    {
        vTaskDelay(pdMS_TO_TICKS(1)); 
    }
}
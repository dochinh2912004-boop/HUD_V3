#include "lib_gpio.h"
#include "driver/gpio.h"
#include <string.h>  
#include "esp_system.h"

Button_t button{};

void GPIO_Setup(void)
{
    // Khởi tạo chân LED
    // gpio_config_t io_conf;
    // memset(&io_conf, 0, sizeof(io_conf));
    // io_conf.pin_bit_mask = (1ULL << LED_BLINK_PIN);
    // io_conf.mode         = GPIO_MODE_OUTPUT;
    // io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    // io_conf.pull_up_en   = GPIO_PULLUP_DISABLE;
    // io_conf.intr_type    = GPIO_INTR_DISABLE;
    // gpio_config(&io_conf);

    // Khởi tạo chân Nút Bấm kéo trở lên cao (Pull-up)
    gpio_config_t button_conf;
    memset(&button_conf, 0, sizeof(button_conf));
    button_conf.pin_bit_mask = (1ULL << BUTTON_PIN);
    button_conf.mode         = GPIO_MODE_INPUT;
    button_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    button_conf.pull_up_en   = GPIO_PULLUP_DISABLE;
    button_conf.intr_type    = GPIO_INTR_DISABLE;
    gpio_config(&button_conf);

    // gpio_set_level(LED_BLINK_PIN, 1);
}

void BUTTON_ReadAll(void)
{
    BUTTON_Process(&button, BUTTON_READ);
}


void BUTTON_Process(Button_t *button, uint8_t state)
{
    if (!state) // Nếu nút bấm đang được nhấn xuống (mức thấp 0)
    {
        button->counter++;

        // Nhấn giữ đủ 1000ms (1 giây) và chưa từng kích hoạt Hold trước đó
        if ((button->counter >= 2000) && (button->flag_hold_triggered == 0))
        {   
            // esp_restart();
            button->flag_hold = 1;
            button->flag_hold_triggered = 1; // Đánh dấu đã kích hoạt Hold lần này
            
        }
        else if((button->counter >= 5000) && (button->flag_hold_triggered == 1)){
             esp_restart();
        }
    }
    else // Nếu nút bấm đã được thả ra (mức cao 1)
    {
        // Nếu trước đó có nhấn xuống nhưng chưa đủ thời gian làm cờ Hold kích hoạt
        if ((button->counter > 20) && (button->flag_hold_triggered == 0))
        {
            button->flag_press = 1; // Kích hoạt cờ nhấn nhả nhanh (Press)
        }
        
        // Reset toàn bộ bộ đếm về trạng thái ban đầu để chuẩn bị cho lượt bấm tiếp theo
        button->counter = 0;
        button->flag_hold_triggered = 0;
    }
    button->state_old = state;
}
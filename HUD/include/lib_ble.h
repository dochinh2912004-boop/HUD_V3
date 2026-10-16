#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

extern volatile uint32_t g_speed_limit_kmh;
extern volatile bool     g_speed_limit_valid;

void wyn_ble_peripheral_init(void);
bool camera_led_should_be_on(void);
bool no_overtaking_led_should_be_on(void);

#ifdef __cplusplus
}
#endif
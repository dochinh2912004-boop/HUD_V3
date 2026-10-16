#ifndef _LIB_ADC_H_
#define _LIB_ADC_H_

#include <cstdint>

/* ESP32 ADC Configuration */
#define ADC_CHANNEL             ADC_CHANNEL_2     // GPIO36 (VP)

#define ADC_ATTEN               ADC_ATTEN_DB_12    // Cập nhật từ DB_11 thành DB_12
extern volatile uint16_t data_adc[1];

void DMA_Setup(void);
void ADC1_Setup(void);
uint16_t ADC1_Read(void);
void ADC1_Deinit(void);

#endif

#include "lib_adc.h"
#include "esp_adc/adc_oneshot.h"
#include <string.h>

volatile uint16_t data_adc[1] = {0};

static adc_oneshot_unit_handle_t adc_handle = NULL; 

void ADC1_Setup(void)
{
    adc_oneshot_unit_init_cfg_t init_config;
    memset(&init_config, 0, sizeof(init_config)); 
    
    init_config.unit_id = ADC_UNIT_1;
    init_config.ulp_mode = ADC_ULP_MODE_DISABLE; 
    
    adc_oneshot_new_unit(&init_config, &adc_handle);
    adc_oneshot_chan_cfg_t config;
    memset(&config, 0, sizeof(config));
    
    config.atten = ADC_ATTEN_DB_12;       
    config.bitwidth = ADC_BITWIDTH_12;    
    
    adc_oneshot_config_channel(adc_handle, (adc_channel_t)ADC_CHANNEL, &config);
}

uint16_t ADC1_Read(void)
{
    if (adc_handle == NULL) return 0;
    
    int adc_raw = 0;
    adc_oneshot_read(adc_handle, (adc_channel_t)ADC_CHANNEL, &adc_raw);
    data_adc[0] = (uint16_t)adc_raw;
    return data_adc[0];
}

void ADC1_Deinit(void)
{
    if (adc_handle != NULL) {
        adc_oneshot_del_unit(adc_handle);
        adc_handle = NULL;
    }
}
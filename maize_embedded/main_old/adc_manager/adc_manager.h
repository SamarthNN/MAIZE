#ifndef ADC_MANAGER_H
#define ADC_MANAGER_H

#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"

esp_err_t adc_manager_init(void);

esp_err_t adc_manager_read(
    adc_channel_t channel,
    int *raw,
    int *voltage_mv
);

#endif

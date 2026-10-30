#pragma once

#include "esp_err.h"
#include <stdint.h>

typedef enum {
    LED_STATUS_OK = 0,
    LED_ERROR_I2C = 1,
    LED_ERROR_UART = 2,
    LED_ERROR_SENSOR_INIT = 3,
    LED_ERROR_SENSOR_READ = 4,
    LED_ERROR_UART_TX = 5
} led_status_t;

esp_err_t led_warning_init(void);
void led_warning_set_status(led_status_t status);

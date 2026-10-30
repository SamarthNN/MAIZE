#ifndef VOLTAGE_SENSOR_H
#define VOLTAGE_SENSOR_H

#include "esp_err.h"

esp_err_t voltage_sensor_init(void);

esp_err_t voltage_sensor_read(
    float *voltage
);

#endif

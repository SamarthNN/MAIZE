#ifndef NPK_SENSOR_H
#define NPK_SENSOR_H

#include <stdint.h>
#include "esp_err.h"

typedef struct
{
    float moisture_percent;
    float temperature_c;
    float ph;
    uint16_t conductivity_us_cm;

    uint16_t nitrogen_mg_kg;
    uint16_t phosphorus_mg_kg;
    uint16_t potassium_mg_kg;

} npk_sensor_data_t;

esp_err_t npk_sensor_init(void);

esp_err_t npk_sensor_read(npk_sensor_data_t *data);

#endif

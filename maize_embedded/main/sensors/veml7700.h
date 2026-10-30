#ifndef VEML7700_H
#define VEML7700_H

#include "esp_err.h"
#include "driver/i2c_master.h"

#define VEML7700_I2C_ADDR 0x10

esp_err_t veml7700_init(i2c_master_bus_handle_t bus_handle);
esp_err_t veml7700_read(float *lux);

#endif /* VEML7700_H */

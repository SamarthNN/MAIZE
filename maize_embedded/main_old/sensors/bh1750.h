#ifndef BH1750_H
#define BH1750_H

#include "esp_err.h"
#include "driver/i2c_master.h"

#define BH1750_I2C_ADDR 0x23

esp_err_t bh1750_init(
    i2c_master_bus_handle_t bus_handle
);

esp_err_t bh1750_read(
    float *lux
);

#endif

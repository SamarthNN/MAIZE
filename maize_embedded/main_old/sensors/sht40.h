#ifndef SHT40_H
#define SHT40_H

#include "esp_err.h"
#include "driver/i2c_master.h"

#define SHT40_I2C_ADDR 0x44


esp_err_t sht40_init(
    i2c_master_bus_handle_t bus_handle
);


esp_err_t sht40_read(
    float *temperature,
    float *humidity
);


#endif

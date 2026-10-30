#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

/* =========================================================
 * Sensor Data
 * ========================================================= */

typedef struct
{
    /* Existing sensors */
    float temperature;
    float humidity;

    float light_lux;

    float uv_index;
    float uv_voltage;

    float input_voltage;

    uint16_t co2_ppm;

    /* NPK soil sensor */
    float soil_moisture_percent;
    float soil_temperature_c;
    float soil_ph;

    uint16_t soil_conductivity_us_cm;

    uint16_t nitrogen_mg_kg;
    uint16_t phosphorus_mg_kg;
    uint16_t potassium_mg_kg;

} sensor_data_t;

/* =========================================================
 * Sensor Manager API
 * ========================================================= */

/**
 * Initialize all sensors.
 *
 * @param i2c_bus Shared I2C bus: SDA GPIO8, SCL GPIO9.
 * @param veml7700_bus VEML7700 I2C bus: SDA GPIO11, SCL GPIO12.
 *
 * @return ESP_OK on success.
 */
esp_err_t sensor_manager_init(
    i2c_master_bus_handle_t i2c_bus,
    i2c_master_bus_handle_t veml7700_bus
);

/**
 * Read all sensors.
 *
 * @param data Pointer to sensor data structure.
 *
 * @return ESP_OK on success.
 */
esp_err_t sensor_manager_read(
    sensor_data_t *data
);

#endif /* SENSOR_MANAGER_H */

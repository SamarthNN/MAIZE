#include "sht40.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define TAG "SHT40"

#define SHT40_MEASURE_HIGH_PRECISION    0xFD

static i2c_master_dev_handle_t sht40_handle;


/* --------------------------------------------------
 * Initialize SHT40
 * -------------------------------------------------- */

esp_err_t sht40_init(i2c_master_bus_handle_t bus_handle)
{
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SHT40_I2C_ADDR,
        .scl_speed_hz = 100000,
    };

    esp_err_t ret = i2c_master_bus_add_device(
        bus_handle,
        &dev_config,
        &sht40_handle
    );

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SHT40 to I2C bus");
        return ret;
    }

    ESP_LOGI(TAG, "SHT40 initialized");

    return ESP_OK;
}


/* --------------------------------------------------
 * Read SHT40
 * -------------------------------------------------- */

esp_err_t sht40_read(float *temperature, float *humidity)
{
    uint8_t command = SHT40_MEASURE_HIGH_PRECISION;
    uint8_t data[6];

    /*
     * Send measurement command
     */
    esp_err_t ret = i2c_master_transmit(
        sht40_handle,
        &command,
        1,
        1000
    );

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send measurement command");
        return ret;
    }


    /*
     * Wait for measurement
     */
    vTaskDelay(pdMS_TO_TICKS(10));


    /*
     * Read 6 bytes
     *
     * Byte 0: Temperature MSB
     * Byte 1: Temperature LSB
     * Byte 2: Temperature CRC
     *
     * Byte 3: Humidity MSB
     * Byte 4: Humidity LSB
     * Byte 5: Humidity CRC
     */

    ret = i2c_master_receive(
        sht40_handle,
        data,
        sizeof(data),
        1000
    );

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read sensor data");
        return ret;
    }


    /*
     * Convert temperature raw value
     */

    uint16_t raw_temperature =
        ((uint16_t)data[0] << 8) |
        data[1];


    /*
     * Convert humidity raw value
     */

    uint16_t raw_humidity =
        ((uint16_t)data[3] << 8) |
        data[4];


    /*
     * SHT40 temperature formula
     *
     * T = -45 + 175 * raw / 65535
     */

    *temperature =
        -45.0f +
        175.0f *
        ((float)raw_temperature / 65535.0f);


    /*
     * SHT40 humidity formula
     *
     * RH = -6 + 125 * raw / 65535
     */

    *humidity =
        -6.0f +
        125.0f *
        ((float)raw_humidity / 65535.0f);


    /*
     * Clamp humidity to 0-100%
     */

    if (*humidity < 0.0f)
        *humidity = 0.0f;

    if (*humidity > 100.0f)
        *humidity = 100.0f;


    return ESP_OK;
}

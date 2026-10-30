#include "veml7700.h"

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define TAG "VEML7700"

/* VEML7700 register addresses */
#define REG_ALS_CONF  0x00
#define REG_ALS_DATA  0x04

/*
 * Configuration 0x0000:
 * gain = 1/8, integration time = 100 ms, ALS enabled.
 * At this setting, lux = raw ALS count * 0.0576.
 */
#define ALS_CONF_DEFAULT 0x0000
#define ALS_LUX_PER_COUNT 0.0576f

static i2c_master_dev_handle_t veml7700_handle;
static bool veml7700_initialized = false;

static esp_err_t veml7700_write_register(uint8_t reg, uint16_t value)
{
    uint8_t tx[3] = {
        reg,
        (uint8_t)(value & 0xFF),
        (uint8_t)((value >> 8) & 0xFF)
    };

    return i2c_master_transmit(veml7700_handle, tx, sizeof(tx), 1000);
}

static esp_err_t veml7700_read_register(uint8_t reg, uint16_t *value)
{
    if (value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t rx[2] = {0};
    esp_err_t ret = i2c_master_transmit_receive(
        veml7700_handle, &reg, 1, rx, sizeof(rx), 1000
    );

    if (ret != ESP_OK) {
        return ret;
    }

    /* VEML7700 register data is transmitted low byte first. */
    *value = (uint16_t)rx[0] | ((uint16_t)rx[1] << 8);
    return ESP_OK;
}

esp_err_t veml7700_init(i2c_master_bus_handle_t bus_handle)
{
    if (bus_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = VEML7700_I2C_ADDR,
        .scl_speed_hz = 100000,
    };

    esp_err_t ret = i2c_master_bus_add_device(
        bus_handle, &dev_config, &veml7700_handle
    );
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add I2C device: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = veml7700_write_register(REG_ALS_CONF, ALS_CONF_DEFAULT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure sensor: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Allow the first 100 ms integration cycle to complete. */
    vTaskDelay(pdMS_TO_TICKS(120));
    veml7700_initialized = true;

    ESP_LOGI(TAG, "Initialized at address 0x%02X (GPIO11/12 bus)",
             VEML7700_I2C_ADDR);
    return ESP_OK;
}

esp_err_t veml7700_read(float *lux)
{
    if (lux == NULL || !veml7700_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    uint16_t raw = 0;
    esp_err_t ret = veml7700_read_register(REG_ALS_DATA, &raw);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ALS data read failed: %s", esp_err_to_name(ret));
        return ret;
    }

    *lux = (float)raw * ALS_LUX_PER_COUNT;

    ESP_LOGD(TAG, "Ambient Light: %.2f lux | Raw ALS: %u",
             *lux, (unsigned)raw);
    return ESP_OK;
}

#include "bh1750.h"

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#define TAG "BH1750"

#define BH1750_POWER_ON             0x01
#define BH1750_RESET                0x07
#define BH1750_CONTINUOUS_HIGH_RES  0x10

#define BH1750_MEASUREMENT_MS       180
#define BH1750_RETRY_DELAY_MS       20

static i2c_master_dev_handle_t bh1750_handle;
static bool bh1750_initialized = false;


/*
 * Send a single BH1750 command.
 */
static esp_err_t bh1750_send_command(uint8_t command)
{
    return i2c_master_transmit(
        bh1750_handle,
        &command,
        1,
        1000
    );
}


/*
 * Initialize the BH1750.
 */
esp_err_t bh1750_init(i2c_master_bus_handle_t bus_handle)
{
    if (bus_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BH1750_I2C_ADDR,
        .scl_speed_hz = 100000,
    };

    esp_err_t ret = i2c_master_bus_add_device(
        bus_handle,
        &dev_config,
        &bh1750_handle
    );

    if (ret != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed to add device: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }

    ret = bh1750_send_command(BH1750_POWER_ON);

    if (ret != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Power-on command failed: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(10));

    ret = bh1750_send_command(BH1750_RESET);

    if (ret != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Reset command failed: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }

    ret = bh1750_send_command(
        BH1750_CONTINUOUS_HIGH_RES
    );

    if (ret != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Measurement command failed: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }

    /*
     * Wait for the first measurement to complete.
     */
    vTaskDelay(pdMS_TO_TICKS(BH1750_MEASUREMENT_MS));

    bh1750_initialized = true;

    ESP_LOGI(
        TAG,
        "Initialized at address 0x%02X",
        BH1750_I2C_ADDR
    );

    return ESP_OK;
}


/*
 * Read the current light measurement.
 */
esp_err_t bh1750_read(float *lux)
{
    if (lux == NULL || !bh1750_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t data[2] = {0};

    esp_err_t ret = i2c_master_receive(
        bh1750_handle,
        data,
        sizeof(data),
        1000
    );

    if (ret != ESP_OK) {
        ESP_LOGW(
            TAG,
            "I2C read failed: %s",
            esp_err_to_name(ret)
        );

        /*
         * Wait before retrying, so a transient failure
         * does not immediately trigger another transaction.
         */
        vTaskDelay(
            pdMS_TO_TICKS(BH1750_RETRY_DELAY_MS)
        );

        ret = i2c_master_receive(
            bh1750_handle,
            data,
            sizeof(data),
            1000
        );

        if (ret != ESP_OK) {
            ESP_LOGE(
                TAG,
                "Read retry failed: %s",
                esp_err_to_name(ret)
            );

            return ret;
        }
    }

    uint16_t raw_value =
        ((uint16_t)data[0] << 8) |
        (uint16_t)data[1];

    /*
     * A valid reading can be zero in darkness.
     * Do not reject zero as an invalid measurement.
     */
    *lux = (float)raw_value / 1.2f;

    ESP_LOGD(
        TAG,
        "RAW: %u | Lux: %.2f",
        (unsigned)raw_value,
        *lux
    );

    return ESP_OK;
}

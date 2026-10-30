#include "npk_sensor.h"

#include <string.h>

#include "driver/uart.h"
#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#define TAG "NPK_SENSOR"

/* UART configuration */
#define NPK_UART_NUM       UART_NUM_2
#define NPK_RX_PIN         GPIO_NUM_16
#define NPK_TX_PIN         GPIO_NUM_17
#define NPK_BAUD_RATE      4800

/* Modbus configuration */
#define NPK_SLAVE_ID       0x01
#define MODBUS_READ_HOLDING_REGISTERS 0x03

#define NPK_START_REGISTER 0x0000
#define NPK_REGISTER_COUNT 7

#define NPK_REQUEST_LENGTH  8
#define NPK_RESPONSE_LENGTH 19

#define NPK_RESPONSE_TIMEOUT_MS 1500

static const uint8_t request_frame[NPK_REQUEST_LENGTH] =
{
    0x01, 0x03, 0x00, 0x00,
    0x00, 0x07, 0x04, 0x08
};

static bool npk_initialized = false;


/* --------------------------------------------------
 * Modbus RTU CRC-16
 * -------------------------------------------------- */

static uint16_t calculate_crc(
    const uint8_t *data,
    size_t length
)
{
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < length; i++)
    {
        crc ^= data[i];

        for (int bit = 0; bit < 8; bit++)
        {
            if (crc & 1)
            {
                crc = (crc >> 1) ^ 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc;
}


/* --------------------------------------------------
 * Extract a 16-bit register
 * -------------------------------------------------- */

static uint16_t read_register(
    const uint8_t *response,
    uint8_t register_index
)
{
    size_t offset = 3 + (register_index * 2);

    return ((uint16_t)response[offset] << 8)
           | response[offset + 1];
}


/* --------------------------------------------------
 * Initialize NPK UART
 * -------------------------------------------------- */

esp_err_t npk_sensor_init(void)
{
    ESP_LOGI(TAG, "Initializing NPK Modbus RTU");

    ESP_LOGI(
        TAG,
        "UART%d | RX: GPIO%d | TX: GPIO%d | Baud: %d",
        (int)NPK_UART_NUM,
        (int)NPK_RX_PIN,
        (int)NPK_TX_PIN,
        NPK_BAUD_RATE
    );

    uart_config_t uart_config =
    {
        .baud_rate = NPK_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_driver_install(
        NPK_UART_NUM,
        256,
        0,
        0,
        NULL,
        0
    );

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "UART driver installation failed: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }

    ret = uart_param_config(
        NPK_UART_NUM,
        &uart_config
    );

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "UART configuration failed: %s",
            esp_err_to_name(ret)
        );

        uart_driver_delete(NPK_UART_NUM);
        return ret;
    }

    ret = uart_set_pin(
        NPK_UART_NUM,
        NPK_TX_PIN,
        NPK_RX_PIN,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE
    );

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "UART pin configuration failed: %s",
            esp_err_to_name(ret)
        );

        uart_driver_delete(NPK_UART_NUM);
        return ret;
    }

    uart_flush_input(NPK_UART_NUM);

    npk_initialized = true;

    ESP_LOGI(
        TAG,
        "NPK UART initialized successfully"
    );

    return ESP_OK;
}


/* --------------------------------------------------
 * Read NPK sensor using Modbus RTU
 * -------------------------------------------------- */

esp_err_t npk_sensor_read(npk_sensor_data_t *data)
{
    if (data == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!npk_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t response[NPK_RESPONSE_LENGTH] = {0};

    /*
     * Clear stale bytes before sending a request.
     */
    uart_flush_input(NPK_UART_NUM);

    /*
     * Send Modbus request.
     */
    int written = uart_write_bytes(
        NPK_UART_NUM,
        request_frame,
        sizeof(request_frame)
    );

    if (written != NPK_REQUEST_LENGTH)
    {
        ESP_LOGE(TAG, "Failed to send complete Modbus request");
        return ESP_FAIL;
    }

    /*
     * Wait until the request has physically left the UART.
     * The converter handles RS-485 direction automatically.
     */
    esp_err_t ret = uart_wait_tx_done(
        NPK_UART_NUM,
        pdMS_TO_TICKS(500)
    );

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "UART TX timeout: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }

    /*
     * Read the response.
     */
    int received = uart_read_bytes(
        NPK_UART_NUM,
        response,
        sizeof(response),
        pdMS_TO_TICKS(NPK_RESPONSE_TIMEOUT_MS)
    );

    if (received != NPK_RESPONSE_LENGTH)
    {
        ESP_LOGW(
            TAG,
            "Incomplete Modbus response: %d/%d bytes",
            received,
            NPK_RESPONSE_LENGTH
        );

        return ESP_ERR_TIMEOUT;
    }

    /*
     * Print raw response for debugging.
     */
    ESP_LOG_BUFFER_HEX(
        TAG,
        response,
        sizeof(response)
    );

    /*
     * Validate slave ID.
     */
    if (response[0] != NPK_SLAVE_ID)
    {
        ESP_LOGE(
            TAG,
            "Incorrect slave ID: 0x%02X",
            response[0]
        );

        return ESP_ERR_INVALID_RESPONSE;
    }

    /*
     * Detect Modbus exception responses.
     */
    if (response[1] & 0x80)
    {
        ESP_LOGE(
            TAG,
            "Modbus exception: function=0x%02X code=0x%02X",
            response[1],
            response[2]
        );

        return ESP_FAIL;
    }

    /*
     * Validate function code.
     */
    if (response[1] != MODBUS_READ_HOLDING_REGISTERS)
    {
        ESP_LOGE(
            TAG,
            "Unexpected function code: 0x%02X",
            response[1]
        );

        return ESP_ERR_INVALID_RESPONSE;
    }

    /*
     * Seven registers must produce 14 data bytes.
     */
    if (response[2] != NPK_REGISTER_COUNT * 2)
    {
        ESP_LOGE(
            TAG,
            "Unexpected Modbus byte count: %u",
            response[2]
        );

        return ESP_ERR_INVALID_RESPONSE;
    }

    /*
     * Validate CRC.
     *
     * The last two bytes are transmitted low byte first.
     */
    uint16_t calculated_crc = calculate_crc(
        response,
        NPK_RESPONSE_LENGTH - 2
    );

    uint16_t received_crc =
        response[NPK_RESPONSE_LENGTH - 2]
        | ((uint16_t)response[NPK_RESPONSE_LENGTH - 1] << 8);

    if (calculated_crc != received_crc)
    {
        ESP_LOGE(
            TAG,
            "CRC mismatch: calculated=0x%04X received=0x%04X",
            calculated_crc,
            received_crc
        );

        return ESP_ERR_INVALID_CRC;
    }

    /*
     * Extract registers.
     *
     * Response offsets:
     * 0: slave ID
     * 1: function
     * 2: byte count
     * 3-16: seven registers
     * 17-18: CRC
     */

    uint16_t raw_moisture = read_register(response, 0);
    uint16_t raw_temperature = read_register(response, 1);
    uint16_t raw_conductivity = read_register(response, 2);
    uint16_t raw_ph = read_register(response, 3);

    /*
     * Some soil sensors encode negative temperature as
     * a signed 16-bit two's-complement value.
     */
    int16_t signed_temperature = (int16_t)raw_temperature;

    uint16_t raw_nitrogen = read_register(response, 4);
    uint16_t raw_phosphorus = read_register(response, 5);
    uint16_t raw_potassium = read_register(response, 6);

    /*
     * Convert values using the scaling assumed by
     * the existing Arduino implementation.
     */
    data->moisture_percent = raw_moisture / 10.0f;

    data->temperature_c = signed_temperature / 10.0f;

    data->conductivity_us_cm = raw_conductivity;

    data->ph = raw_ph / 10.0f;

    data->nitrogen_mg_kg = raw_nitrogen;
    data->phosphorus_mg_kg = raw_phosphorus;
    data->potassium_mg_kg = raw_potassium;

    ESP_LOGI(
        TAG,
        "Moisture: %.1f%% | Soil Temp: %.1f C | "
        "EC: %u uS/cm | pH: %.1f | "
        "N: %u | P: %u | K: %u mg/kg",
        data->moisture_percent,
        data->temperature_c,
        data->conductivity_us_cm,
        data->ph,
        data->nitrogen_mg_kg,
        data->phosphorus_mg_kg,
        data->potassium_mg_kg
    );

    return ESP_OK;
}

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c_master.h"
#include "driver/uart.h"

#include "esp_log.h"
#include "esp_err.h"

#include "sensor_manager/sensor_manager.h"
#include "sensors/mhz19b.h"
#include "led_warning.h"


/* ==================================================
 * LOGGING
 * ================================================== */

#define TAG "MAIN"
#define UART_TAG "UART_SENDER"


/* ==================================================
 * I2C CONFIGURATION
 * ================================================== */

#define I2C_PORT        I2C_NUM_0
#define I2C_SDA_GPIO    8
#define I2C_SCL_GPIO    9
#define I2C_FREQ_HZ     100000

static i2c_master_bus_handle_t i2c_bus;
static i2c_master_bus_handle_t veml7700_i2c_bus;

/* ==================================================
 * UART CONFIGURATION
 * ESP32-S3 -> ESP32-C3 Mini
 * ================================================== */

#define PI_UART             UART_NUM_1
#define PI_UART_TX_PIN      GPIO_NUM_14
#define PI_UART_BAUD        115200

#define UART_PACKET_SIZE    37

static bool uart_ready = false;
static uint16_t packet_sequence = 0;


/* ==================================================
 * I2C INITIALIZATION
 * ================================================== */

static esp_err_t i2c_init(void)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_PORT,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t err = i2c_new_master_bus(&bus_config, &i2c_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus initialization failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "I2C bus initialized: SDA GPIO%d, SCL GPIO%d, %d Hz",
             I2C_SDA_GPIO, I2C_SCL_GPIO, I2C_FREQ_HZ);
    return ESP_OK;
}


/* ==================================================
 * VEML7700 I2C INITIALIZATION
 * SDA GPIO11, SCL GPIO12 (separate bus)
 * ================================================== */

static esp_err_t veml7700_i2c_init(void)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_1,
        .sda_io_num = GPIO_NUM_11,
        .scl_io_num = GPIO_NUM_12,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&bus_config, &veml7700_i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "VEML7700 I2C bus initialization failed: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "VEML7700 I2C initialized: SDA GPIO11, SCL GPIO12");
    return ESP_OK;
}


/* ==================================================
 * I2C SCANNER
 * Returns true when the expected SHT40 and VEML7700 are detected.
 * VEML7700 uses fixed 7-bit address 0x10.
 * ================================================== */

static bool i2c_scan_expected_sensors(void)
{
    bool sht40_found = false;
    bool veml7700_found = false;
    int main_bus_devices = 0;
    int veml7700_bus_devices = 0;

    ESP_LOGI(TAG, "Scanning SHT40 I2C bus (GPIO8/9)...");

    for (uint8_t addr = 1; addr < 127; addr++) {
        esp_err_t ret = i2c_master_probe(i2c_bus, addr, 100);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Main I2C device found at 0x%02X", addr);
            main_bus_devices++;
            if (addr == 0x44) {
                sht40_found = true;
            }
        }
    }

    ESP_LOGI(TAG, "Main I2C scan complete: %d device(s)", main_bus_devices);

    ESP_LOGI(TAG, "Scanning VEML7700 I2C bus (GPIO11/12)...");

    for (uint8_t addr = 1; addr < 127; addr++) {
        esp_err_t ret = i2c_master_probe(veml7700_i2c_bus, addr, 100);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "VEML7700 bus device found at 0x%02X", addr);
            veml7700_bus_devices++;
            if (addr == 0x10) {
                veml7700_found = true;
            }
        }
    }

    ESP_LOGI(TAG, "VEML7700 I2C scan complete: %d device(s)",
             veml7700_bus_devices);

    if (!sht40_found) {
        ESP_LOGE(TAG, "SHT40 not detected at 0x44 on GPIO8/9");
    }
    if (!veml7700_found) {
        ESP_LOGE(TAG, "VEML7700 not detected at 0x10 on GPIO11/12");
    }

    return sht40_found && veml7700_found;
}

/* ==================================================
 * UART INITIALIZATION
 *
 * Format: 115200 baud, 8E1
 * 8 data bits, even parity, 1 stop bit
 * ================================================== */

static esp_err_t sender_uart_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = PI_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_EVEN,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

esp_err_t ret = uart_driver_install(
    PI_UART,
    256,   // RX buffer
    0,     // TX ring buffer disabled
    0,     // Event queue disabled
    NULL,
    0
);
    if (ret != ESP_OK)
    {
        ESP_LOGE(
            UART_TAG,
            "UART driver installation failed: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }

    ret = uart_param_config(
        PI_UART,
        &uart_config
    );

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            UART_TAG,
            "UART configuration failed: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }

    ret = uart_set_pin(
        PI_UART,
        PI_UART_TX_PIN,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE
    );

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            UART_TAG,
            "UART pin configuration failed: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }

    uart_ready = true;

    ESP_LOGI(
        UART_TAG,
        "UART initialized: TX GPIO%d | %d baud | 8E1",
        PI_UART_TX_PIN,
        PI_UART_BAUD
    );

    return ESP_OK;
}


/* ==================================================
 * BINARY SERIALIZATION HELPERS
 *
 * All multibyte fields use little-endian order.
 * ================================================== */

static void put_u16(
    uint8_t *buffer,
    size_t *index,
    uint16_t value
)
{
    buffer[(*index)++] = (uint8_t)(value & 0xFF);
    buffer[(*index)++] = (uint8_t)((value >> 8) & 0xFF);
}


static void put_i16(
    uint8_t *buffer,
    size_t *index,
    int16_t value
)
{
    put_u16(buffer, index, (uint16_t)value);
}


static void put_u32(
    uint8_t *buffer,
    size_t *index,
    uint32_t value
)
{
    buffer[(*index)++] = (uint8_t)(value & 0xFF);
    buffer[(*index)++] = (uint8_t)((value >> 8) & 0xFF);
    buffer[(*index)++] = (uint8_t)((value >> 16) & 0xFF);
    buffer[(*index)++] = (uint8_t)((value >> 24) & 0xFF);
}


/*
 * Convert a float into a scaled unsigned 16-bit value.
 * Invalid or negative values become zero.
 */
static uint16_t float_to_u16(
    float value,
    float scale
)
{
    if (!isfinite(value) || value <= 0.0f)
    {
        return 0;
    }

    float scaled = value * scale;

    if (scaled >= 65535.0f)
    {
        return UINT16_MAX;
    }

    return (uint16_t)(scaled + 0.5f);
}


/*
 * Convert a float into a scaled signed 16-bit value.
 */
static int16_t float_to_i16(
    float value,
    float scale
)
{
    if (!isfinite(value))
    {
        return 0;
    }

    float scaled = value * scale;

    if (scaled > 32767.0f)
    {
        return INT16_MAX;
    }

    if (scaled < -32768.0f)
    {
        return INT16_MIN;
    }

    return (int16_t)(
        scaled >= 0.0f
        ? scaled + 0.5f
        : scaled - 0.5f
    );
}


/* ==================================================
 * CRC-16/MODBUS
 *
 * Initial value: 0xFFFF
 * Polynomial:    0xA001
 * ================================================== */

static uint16_t calculate_crc16(
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


/* ==================================================
 * BUILD BINARY SENSOR PACKET
 *
 * Layout:
 *
 * 0-1   : Sync marker AA 55
 * 2     : Protocol version
 * 3-4   : Sequence number
 * 5-6   : Air temperature, C * 100, signed
 * 7-8   : Humidity, % * 100
 * 9-12  : Light, lux * 10
 * 13-14 : UV index * 100
 * 15-16 : UV voltage, mV
 * 17-18 : Input voltage, mV
 * 19-20 : CO2, ppm
 * 21-22 : Soil moisture, % * 10
 * 23-24 : Soil temperature, C * 10, signed
 * 25-26 : Soil EC, uS/cm
 * 27-28 : Soil pH * 10
 * 29-30 : Nitrogen, mg/kg
 * 31-32 : Phosphorus, mg/kg
 * 33-34 : Potassium, mg/kg
 * 35-36 : CRC-16
 *
 * Total: 37 bytes
 * ================================================== */

static size_t build_sensor_packet(
    const sensor_data_t *data,
    uint8_t *packet
)
{
    size_t index = 0;

    /* Sync marker */
    packet[index++] = 0xAA;
    packet[index++] = 0x55;

    /* Protocol version */
    packet[index++] = 0x01;

    /* Sequence number */
    put_u16(
        packet,
        &index,
        packet_sequence
    );

    /* Air temperature: 0.01 C resolution */
    put_i16(
        packet,
        &index,
        float_to_i16(data->temperature, 100.0f)
    );

    /* Humidity: 0.01% resolution */
    put_u16(
        packet,
        &index,
        float_to_u16(data->humidity, 100.0f)
    );

    /* Light: 0.1 lux resolution */
    uint32_t light_value = 0;

    if (isfinite(data->light_lux) &&
        data->light_lux > 0.0f)
    {
        double scaled = (double)data->light_lux * 10.0;

        if (scaled >= 4294967295.0)
        {
            light_value = UINT32_MAX;
        }
        else
        {
            light_value = (uint32_t)(scaled + 0.5);
        }
    }

    put_u32(
        packet,
        &index,
        light_value
    );

    /* UV index: 0.01 resolution */
    put_u16(
        packet,
        &index,
        float_to_u16(data->uv_index, 100.0f)
    );

    /* UV voltage: millivolts */
    put_u16(
        packet,
        &index,
        float_to_u16(data->uv_voltage, 1000.0f)
    );

    /* Input voltage: millivolts */
    put_u16(
        packet,
        &index,
        float_to_u16(data->input_voltage, 1000.0f)
    );

    /* CO2: ppm */
    put_u16(
        packet,
        &index,
        data->co2_ppm
    );

    /* Soil moisture: 0.1% resolution */
    put_u16(
        packet,
        &index,
        float_to_u16(data->soil_moisture_percent, 10.0f)
    );

    /* Soil temperature: 0.1 C resolution */
    put_i16(
        packet,
        &index,
        float_to_i16(data->soil_temperature_c, 10.0f)
    );

    /* Soil EC: uS/cm */
    put_u16(
        packet,
        &index,
        data->soil_conductivity_us_cm
    );

    /* Soil pH: 0.1 resolution */
    put_u16(
        packet,
        &index,
        float_to_u16(data->soil_ph, 10.0f)
    );

    /* Nitrogen: mg/kg */
    put_u16(
        packet,
        &index,
        data->nitrogen_mg_kg
    );

    /* Phosphorus: mg/kg */
    put_u16(
        packet,
        &index,
        data->phosphorus_mg_kg
    );

    /* Potassium: mg/kg */
    put_u16(
        packet,
        &index,
        data->potassium_mg_kg
    );

    /*
     * CRC covers all preceding bytes.
     * Modbus CRC is transmitted low byte first.
     */
    uint16_t crc = calculate_crc16(
        packet,
        index
    );

    put_u16(
        packet,
        &index,
        crc
    );

    return index;
}


/* ==================================================
 * SEND SENSOR PACKET
 * ================================================== */

static bool send_sensor_packet(const sensor_data_t *data)
{
    if (!uart_ready) {
        ESP_LOGE(UART_TAG, "Cannot send telemetry: UART is not initialized");
        return false;
    }

    uint8_t packet[UART_PACKET_SIZE];
    size_t length = build_sensor_packet(data, packet);

    if (length != UART_PACKET_SIZE) {
        ESP_LOGE(UART_TAG, "Packet length error: got %u, expected %u bytes",
                 (unsigned)length, (unsigned)UART_PACKET_SIZE);
        return false;
    }

    int written = uart_write_bytes(PI_UART, (const char *)packet, length);
    if (written != (int)length) {
        ESP_LOGE(UART_TAG, "UART write failed: %d/%u bytes",
                 written, (unsigned)length);
        return false;
    }

    esp_err_t err = uart_wait_tx_done(PI_UART, pdMS_TO_TICKS(1000));
    if (err != ESP_OK) {
        ESP_LOGE(UART_TAG, "UART transmission timeout: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(UART_TAG, "Sent packet #%u (%u bytes)",
             (unsigned)packet_sequence, (unsigned)length);
    packet_sequence++;
    return true;
}


/* ==================================================
 * MAIN APPLICATION
 * ================================================== */

void app_main(void)
{
    sensor_data_t sensor_data = {0};
    led_status_t startup_fault = LED_STATUS_OK;

    /* Initialize the RGB LED first so subsequent failures can be indicated. */
    esp_err_t led_ret = led_warning_init();
    if (led_ret == ESP_OK) {
        led_warning_set_status(LED_STATUS_OK);
    } else {
        ESP_LOGE(TAG, "RGB LED warning module unavailable: %s", esp_err_to_name(led_ret));
    }

    /* UART: ESP32-S3 GPIO14 TX -> ESP32-C3 GPIO10 RX, 115200 8E1. */
    esp_err_t uart_ret = sender_uart_init();
    if (uart_ret != ESP_OK) {
        startup_fault = LED_ERROR_UART;
        led_warning_set_status(startup_fault);
        ESP_LOGE(TAG, "UART unavailable; local sensor logging will continue");
    } else {
        const uint8_t test_message[] = {'U','A','R','T','_','O','K','\n'};
        int written = uart_write_bytes(PI_UART, (const char *)test_message, sizeof(test_message));
        esp_err_t tx_ret = uart_wait_tx_done(PI_UART, pdMS_TO_TICKS(1000));
        if (written != (int)sizeof(test_message) || tx_ret != ESP_OK) {
            startup_fault = LED_ERROR_UART_TX;
            led_warning_set_status(startup_fault);
            ESP_LOGE(TAG, "UART startup test failed (write=%d/%u, wait=%s)",
                     written, (unsigned)sizeof(test_message), esp_err_to_name(tx_ret));
        } else {
            ESP_LOGI(TAG, "UART test message transmitted");
        }
    }

    /* Initialize the two I2C buses before scanning or initializing sensors. */
    esp_err_t i2c_ret = i2c_init();
    if (i2c_ret != ESP_OK) {
        startup_fault = LED_ERROR_SENSOR_INIT;
        led_warning_set_status(startup_fault);
        ESP_LOGE(TAG, "Main I2C bus initialization failed");
    } else {
        esp_err_t veml7700_bus_ret = veml7700_i2c_init();

        if (veml7700_bus_ret != ESP_OK) {
            startup_fault = LED_ERROR_SENSOR_INIT;
            led_warning_set_status(startup_fault);
            ESP_LOGE(TAG, "VEML7700 I2C bus initialization failed");
        } else {
            bool expected_i2c_sensors_found = i2c_scan_expected_sensors();

            if (!expected_i2c_sensors_found) {
                startup_fault = LED_ERROR_SENSOR_INIT;
                led_warning_set_status(startup_fault);
                ESP_LOGE(TAG, "One or more expected I2C sensors are missing");
            }

            esp_err_t sensor_init_ret = sensor_manager_init(
                i2c_bus,
                veml7700_i2c_bus
            );

            if (sensor_init_ret != ESP_OK) {
                startup_fault = LED_ERROR_SENSOR_INIT;
                led_warning_set_status(startup_fault);
                ESP_LOGE(TAG, "Sensor manager initialization failed: %s",
                         esp_err_to_name(sensor_init_ret));
                ESP_LOGW(TAG, "Continuing with whatever sensors remain usable");
            } else {
                ESP_LOGI(TAG, "Sensor manager initialization returned ESP_OK");
            }
        }
    }

    if (startup_fault == LED_STATUS_OK) {
        led_warning_set_status(LED_STATUS_OK);
        ESP_LOGI(TAG, "Initialization checks passed; warning LED off");
    } else {
        ESP_LOGW(TAG, "Startup issue detected; red LED warning remains active");
    }

    ESP_LOGI(TAG, "Telemetry sampling interval: 5 seconds");

    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t sample_interval = pdMS_TO_TICKS(5000);

    while (1) {
        esp_err_t read_ret = sensor_manager_read(&sensor_data);

        if (read_ret != ESP_OK) {
            ESP_LOGE(TAG, "Sensor read failed: %s", esp_err_to_name(read_ret));
            led_warning_set_status(LED_ERROR_SENSOR_READ);
        } else {
            ESP_LOGI(TAG,
                     "Temperature: %.2f C | Humidity: %.2f %% | Light: %.2f lux | "
                     "UV Index: %.2f | UV Voltage: %.3f V | Input Voltage: %.3f V | CO2: %u ppm",
                     sensor_data.temperature, sensor_data.humidity, sensor_data.light_lux,
                     sensor_data.uv_index, sensor_data.uv_voltage, sensor_data.input_voltage,
                     (unsigned)sensor_data.co2_ppm);

            ESP_LOGI(TAG,
                     "Soil: Moisture %.1f%% | Temperature %.1f C | EC %u uS/cm | "
                     "pH %.1f | N %u | P %u | K %u mg/kg",
                     sensor_data.soil_moisture_percent, sensor_data.soil_temperature_c,
                     (unsigned)sensor_data.soil_conductivity_us_cm, sensor_data.soil_ph,
                     (unsigned)sensor_data.nitrogen_mg_kg, (unsigned)sensor_data.phosphorus_mg_kg,
                     (unsigned)sensor_data.potassium_mg_kg);

            ESP_LOGI(TAG, "MH-Z19B PWM: HIGH: %lu us | PERIOD: %lu us | DUTY: %.2f%%",
                     (unsigned long)mhz19b_get_high_time_us(),
                     (unsigned long)mhz19b_get_period_us(), mhz19b_get_duty());

            if (uart_ready) {
                bool tx_ok = send_sensor_packet(&sensor_data);
                if (!tx_ok) {
                    led_warning_set_status(LED_ERROR_UART_TX);
                } else {
                    /* Keep any startup fault latched; otherwise return to healthy/off. */
                    led_warning_set_status(startup_fault);
                }
            } else {
                led_warning_set_status(LED_ERROR_UART);
            }

            /* Preserve startup warning if initialization checks failed. */
            if (startup_fault != LED_STATUS_OK) {
                led_warning_set_status(startup_fault);
            }
        }

        vTaskDelayUntil(&last_wake, sample_interval);
    }
}


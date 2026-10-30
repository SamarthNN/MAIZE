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

    /* I2C bus initialization is required by sensor_manager. */
    esp_err_t i2c_ret = i2c_init();
    if (i2c_ret != ESP_OK) {
        startup_fault = LED_ERROR_I2C;
        led_warning_set_status(startup_fault);
        ESP_LOGE(TAG, "Cannot initialize sensor manager without an I2C bus");
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    bool expected_i2c_sensors_found = i2c_scan_expected_sensors();
    if (!expected_i2c_sensors_found) {
        startup_fault = LED_ERROR_SENSOR_INIT;
        led_warning_set_status(startup_fault);
        ESP_LOGE(TAG, "One or more expected I2C sensors are missing");
    }

    esp_err_t sensor_init_ret = sensor_manager_init(i2c_bus);
    if (sensor_init_ret != ESP_OK) {
        startup_fault = LED_ERROR_SENSOR_INIT;
        led_warning_set_status(startup_fault);
        ESP_LOGE(TAG, "Sensor manager initialization failed: %s",
                 esp_err_to_name(sensor_init_ret));
        ESP_LOGW(TAG, "Continuing with whatever sensors remain usable");
    } else {
        ESP_LOGI(TAG, "Sensor manager initialization returned ESP_OK");
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

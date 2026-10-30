#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"

#define UV_ADC_UNIT    ADC_UNIT_1
#define UV_ADC_CHANNEL ADC_CHANNEL_3  // GPIO 4 on ESP32-S3
#define UV_SAMPLE_COUNT 16

static const char *TAG = "ML8511_TEST";

void app_main(void)
{
    adc_oneshot_unit_handle_t adc_handle;

    // Initialize ADC1
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = UV_ADC_UNIT,
    };

    ESP_ERROR_CHECK(
        adc_oneshot_new_unit(&init_config, &adc_handle)
    );

    // Configure GPIO 4 / ADC1_CH3
    adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };

    ESP_ERROR_CHECK(
        adc_oneshot_config_channel(
            adc_handle,
            UV_ADC_CHANNEL,
            &channel_config
        )
    );

    ESP_LOGI(TAG, "ML8511 ADC test started on GPIO 4");

    while (1) {
        int raw = 0;
        int sum = 0;

        // Average 16 samples to reduce noise
        for (int i = 0; i < UV_SAMPLE_COUNT; i++) {
            ESP_ERROR_CHECK(
                adc_oneshot_read(
                    adc_handle,
                    UV_ADC_CHANNEL,
                    &raw
                )
            );

            sum += raw;
            vTaskDelay(pdMS_TO_TICKS(2));
        }

        int average_raw = sum / UV_SAMPLE_COUNT;

        // Approximation only; ADC calibration improves accuracy.
        float voltage = (average_raw / 4095.0f) * 3.3f;

        ESP_LOGI(
            TAG,
            "Raw ADC: %d | Approx voltage: %.3f V",
            average_raw,
            voltage
        );

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

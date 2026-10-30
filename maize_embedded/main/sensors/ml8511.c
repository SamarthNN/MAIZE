#include "ml8511.h"

#include <stddef.h>

#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "../adc_manager/adc_manager.h"

#define TAG "ML8511"

/*
 * ESP32-S3 ML8511 configuration
 *
 * ML8511 OUT -> GPIO2 -> ADC1_CHANNEL_1
 *
 * ADC1 is initialized by adc_manager.
 * Do not initialize another ADC unit here.
 */
#define ML8511_ADC_CHANNEL ADC_CHANNEL_1

#define ML8511_SAMPLE_COUNT 20
#define ML8511_SAMPLE_DELAY_MS 10

/*
 * Approximate ML8511 reference points discussed earlier.
 *
 * These are assumed reference points, not a calibrated
 * transfer function for every breakout board.
 */
#define UV_REFERENCE_VOLTAGE   1.00f
#define UV_REFERENCE_INTENSITY 0.00f

#define UV_10MW_VOLTAGE        2.20f
#define UV_10MW_INTENSITY      10.0f


esp_err_t ml8511_init(void)
{
    ESP_LOGI(TAG, "ML8511 initialized");
    ESP_LOGI(TAG, "ML8511 OUT -> GPIO2 -> ADC1_CH1");
    ESP_LOGI(TAG, "UV intensity estimation enabled");

    return ESP_OK;
}


esp_err_t ml8511_read(float *uv_index, float *voltage)
{
    if (uv_index == NULL || voltage == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t total_voltage_mv = 0;

    /*
     * Read multiple samples using the shared ADC manager.
     */
    for (int i = 0; i < ML8511_SAMPLE_COUNT; i++) {

        int raw = 0;
        int voltage_mv = 0;

        esp_err_t ret = adc_manager_read(
            ML8511_ADC_CHANNEL,
            &raw,
            &voltage_mv
        );

        if (ret != ESP_OK) {
            ESP_LOGE(
                TAG,
                "ADC read failed: %s",
                esp_err_to_name(ret)
            );

            return ret;
        }

        total_voltage_mv += (uint32_t)voltage_mv;

        vTaskDelay(pdMS_TO_TICKS(ML8511_SAMPLE_DELAY_MS));
    }

    /*
     * Convert the averaged millivolt reading to volts.
     */
    *voltage =
        ((float)total_voltage_mv / ML8511_SAMPLE_COUNT) / 1000.0f;

    /*
     * Estimate UV intensity using linear interpolation.
     *
     * Output unit: mW/cm^2
     *
     * IMPORTANT:
     * This extrapolates beyond the reference voltage when
     * voltage exceeds 2.20 V. Such results are not validated.
     */
    *uv_index =
        UV_REFERENCE_INTENSITY +
        ((*voltage - UV_REFERENCE_VOLTAGE) /
         (UV_10MW_VOLTAGE - UV_REFERENCE_VOLTAGE)) *
        (UV_10MW_INTENSITY - UV_REFERENCE_INTENSITY);

    if (*uv_index < 0.0f) {
        *uv_index = 0.0f;
    }

    /*
     * Log readings.
     */
    ESP_LOGI(
        TAG,
        "Average voltage: %.3f V | Estimated UV intensity: %.2f mW/cm^2",
        *voltage,
        *uv_index
    );

    /*
     * Diagnostic warning for unexpectedly high voltage.
     */
    if (*voltage < 0.0f || *voltage > 3.3f) {
        ESP_LOGW(
            TAG,
            "Output voltage is outside the expected 0-3.3 V range"
        );
    }

    return ESP_OK;
}

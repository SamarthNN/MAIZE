#include "ml8511.h"

#include "esp_log.h"
#include "../adc_manager/adc_manager.h"

#define TAG "ML8511"

/* GPIO4 = ADC1_CHANNEL_3 */
#define ML8511_ADC_CHANNEL    ADC_CHANNEL_3

/*
 * Approximate ML8511 transfer characteristics.
 *
 * Around zero UV:
 *     VOUT ≈ 1.0 V
 *
 * Approximate slope:
 *     0.4 V / (mW/cm²)
 */

#define ML8511_ZERO_VOLTAGE   1.0f
#define ML8511_V_PER_MW_CM2   0.4f


esp_err_t ml8511_init(void)
{
    /*
     * ADC1 is initialized by adc_manager.
     *
     * DO NOT call adc_oneshot_new_unit() here.
     */

    ESP_LOGI(
        TAG,
        "ML8511 initialized"
    );

    ESP_LOGI(
        TAG,
        "ML8511 OUT -> GPIO4 -> ADC1_CH3"
    );

    return ESP_OK;
}


esp_err_t ml8511_read(
    float *uv_index,
    float *voltage
)
{
    if (
        uv_index == NULL ||
        voltage == NULL
    )
    {
        return ESP_ERR_INVALID_ARG;
    }


    int raw;
    int voltage_mv;


    /*
     * Read ADC1_CH3 through the shared ADC manager.
     */

    esp_err_t ret =
        adc_manager_read(
            ML8511_ADC_CHANNEL,
            &raw,
            &voltage_mv
        );

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "ADC read failed: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }


    /*
     * Convert millivolts to volts.
     */

    *voltage =
        voltage_mv / 1000.0f;


    /*
     * Approximate UV irradiance.
     *
     * Result is in mW/cm².
     */

    float uv_irradiance =
        (*voltage - ML8511_ZERO_VOLTAGE)
        / ML8511_V_PER_MW_CM2;


    /*
     * No negative UV irradiance.
     */

    if (uv_irradiance < 0.0f)
    {
        uv_irradiance = 0.0f;
    }


    /*
     * Approximate UV Index.
     *
     * This is NOT a precision UV-index
     * conversion; it is an approximation.
     */

    *uv_index =
        uv_irradiance * 400.0f;


    ESP_LOGD(
        TAG,
        "RAW: %d | Voltage: %.3f V | "
        "UV irradiance: %.4f mW/cm² | "
        "UV Index: %.2f",
        raw,
        *voltage,
        uv_irradiance,
        *uv_index
    );


    return ESP_OK;
}

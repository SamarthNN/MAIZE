#include "voltage_sensor.h"

#include "esp_log.h"
#include "adc_manager/adc_manager.h"

#define TAG "VOLTAGE_SENSOR"


/* =========================================================
 * ADC Configuration
 * ========================================================= */

/*
 * ESP32-S3:
 *
 * GPIO5 -> ADC1_CHANNEL_4
 */

#define VOLTAGE_ADC_CHANNEL    ADC_CHANNEL_4


/* =========================================================
 * Voltage Divider
 * =========================================================
 *
 *             VIN
 *              |
 *          R1 = 29.75 kΩ
 *              |
 *              +--------> ESP32 ADC (GPIO5)
 *              |
 *          R2 = 7.4 kΩ
 *              |
 *             GND
 *
 *
 * VADC = VIN × R2 / (R1 + R2)
 *
 * VIN = VADC × (R1 + R2) / R2
 *
 * Divider ratio:
 *
 *     (29.75 + 7.40) / 7.40
 *     = 5.0203
 *
 * ========================================================= */

#define VOLTAGE_R1_KOHM    29.75f
#define VOLTAGE_R2_KOHM     7.40f

#define VOLTAGE_DIVIDER_RATIO \
    ((VOLTAGE_R1_KOHM + VOLTAGE_R2_KOHM) / VOLTAGE_R2_KOHM)


/* =========================================================
 * Voltage Calibration
 * =========================================================
 *
 * Reference measurement:
 *
 *     Actual voltage     = 3.916 V
 *     Calculated voltage = 3.770 V
 *
 * Calibration factor:
 *
 *     3.916 / 3.770
 *     = 1.03873
 *
 * Therefore:
 *
 *     Calibrated voltage =
 *         Calculated voltage × 1.03873
 *
 * ========================================================= */

#define VOLTAGE_CALIBRATION_FACTOR    1.03582f


/* =========================================================
 * Initialization
 * ========================================================= */

esp_err_t voltage_sensor_init(void)
{
    ESP_LOGI(
        TAG,
        "Voltage sensor initialized"
    );

    ESP_LOGI(
        TAG,
        "Voltage sensor OUT -> GPIO5 -> ADC1_CH4"
    );

    ESP_LOGI(
        TAG,
        "R1: %.2f kOhm | R2: %.2f kOhm",
        VOLTAGE_R1_KOHM,
        VOLTAGE_R2_KOHM
    );

    ESP_LOGI(
        TAG,
        "Voltage divider ratio: %.4f",
        VOLTAGE_DIVIDER_RATIO
    );

    ESP_LOGI(
        TAG,
        "Calibration factor: %.5f",
        VOLTAGE_CALIBRATION_FACTOR
    );

    return ESP_OK;
}


/* =========================================================
 * Read Input Voltage
 * ========================================================= */

esp_err_t voltage_sensor_read(
    float *voltage
)
{
    if (voltage == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }


    int raw;
    int voltage_mv;


    /* =====================================================
     * Read GPIO5 through shared ADC manager
     * ===================================================== */

    esp_err_t ret =
        adc_manager_read(
            VOLTAGE_ADC_CHANNEL,
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


    /* =====================================================
     * ADC millivolts -> ADC voltage
     * ===================================================== */

    float adc_voltage =
        voltage_mv / 1000.0f;


    /* =====================================================
     * ADC voltage -> input voltage
     * ===================================================== */

    float calculated_voltage =
        adc_voltage *
        VOLTAGE_DIVIDER_RATIO;


    /* =====================================================
     * Apply calibration
     * ===================================================== */

    *voltage =
        calculated_voltage *
        VOLTAGE_CALIBRATION_FACTOR;


    /* =====================================================
     * Debug information
     * ===================================================== */

    ESP_LOGI(
        TAG,
        "RAW: %d | "
        "ADC: %.3f V | "
        "Calculated: %.3f V | "
        "Calibrated: %.3f V",

        raw,
        adc_voltage,
        calculated_voltage,
        *voltage
    );


    return ESP_OK;
}

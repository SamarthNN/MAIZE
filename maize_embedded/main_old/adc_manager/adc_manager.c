#include "adc_manager.h"

#include <stdbool.h>

#include "esp_log.h"
#include "esp_adc/adc_cali_scheme.h"

#define TAG "ADC_MANAGER"

#define ADC_UNIT_USED       ADC_UNIT_1
#define ADC_ATTEN_USED      ADC_ATTEN_DB_12
#define ADC_BITWIDTH_USED   ADC_BITWIDTH_DEFAULT

#define ADC_NUM_SAMPLES     32

static adc_oneshot_unit_handle_t adc_handle = NULL;

static adc_cali_handle_t adc_cali_handle = NULL;

static bool calibration_enabled = false;


/* =========================
 * ADC Calibration
 * ========================= */

static bool adc_calibration_init(void)
{
    esp_err_t ret;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED

    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_USED,
        .atten = ADC_ATTEN_USED,
        .bitwidth = ADC_BITWIDTH_USED,
    };

    ret = adc_cali_create_scheme_curve_fitting(
        &cali_config,
        &adc_cali_handle
    );

    if (ret == ESP_OK)
    {
        ESP_LOGI(
            TAG,
            "ADC curve fitting calibration enabled"
        );

        calibration_enabled = true;

        return true;
    }

    ESP_LOGW(
        TAG,
        "ADC curve fitting calibration unavailable"
    );

#endif


#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED

    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_USED,
        .atten = ADC_ATTEN_USED,
        .bitwidth = ADC_BITWIDTH_USED,
    };

    ret = adc_cali_create_scheme_line_fitting(
        &cali_config,
        &adc_cali_handle
    );

    if (ret == ESP_OK)
    {
        ESP_LOGI(
            TAG,
            "ADC line fitting calibration enabled"
        );

        calibration_enabled = true;

        return true;
    }

    ESP_LOGW(
        TAG,
        "ADC line fitting calibration unavailable"
    );

#endif

    ESP_LOGW(
        TAG,
        "Using uncalibrated ADC readings"
    );

    return false;
}


/* =========================
 * Initialize ADC1
 * ========================= */

esp_err_t adc_manager_init(void)
{
    esp_err_t ret;

    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_USED,
    };

    ret = adc_oneshot_new_unit(
        &init_config,
        &adc_handle
    );

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Failed to initialize ADC1: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }


    adc_calibration_init();


    ESP_LOGI(
        TAG,
        "ADC1 initialized"
    );

    return ESP_OK;
}


/* =========================
 * Read ADC channel
 * ========================= */

esp_err_t adc_manager_read(
    adc_channel_t channel,
    int *raw,
    int *voltage_mv
)
{
    if (
        raw == NULL ||
        voltage_mv == NULL
    )
    {
        return ESP_ERR_INVALID_ARG;
    }


    /*
     * Configure the requested channel.
     *
     * Multiple channels can share the same ADC1
     * unit handle.
     */

    adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN_USED,
        .bitwidth = ADC_BITWIDTH_USED,
    };

    esp_err_t ret =
        adc_oneshot_config_channel(
            adc_handle,
            channel,
            &channel_config
        );

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Failed to configure ADC channel: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }


    /* Take multiple samples */

    int adc_sum = 0;

    for (
        int i = 0;
        i < ADC_NUM_SAMPLES;
        i++
    )
    {
        int sample;

        ret = adc_oneshot_read(
            adc_handle,
            channel,
            &sample
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

        adc_sum += sample;
    }


    *raw =
        adc_sum / ADC_NUM_SAMPLES;


    /* Convert raw ADC to millivolts */

    if (calibration_enabled)
    {
        ret = adc_cali_raw_to_voltage(
            adc_cali_handle,
            *raw,
            voltage_mv
        );

        if (ret != ESP_OK)
        {
            ESP_LOGE(
                TAG,
                "ADC calibration failed: %s",
                esp_err_to_name(ret)
            );

            return ret;
        }
    }
    else
    {
        *voltage_mv =
            (*raw * 3300) / 4095;
    }


    return ESP_OK;
}

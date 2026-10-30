#include "sensor_manager.h"

#include "esp_log.h"

#include "../adc_manager/adc_manager.h"

#include "../sensors/sht40.h"
#include "../sensors/bh1750.h"
#include "../sensors/ml8511.h"
#include "../sensors/voltage_sensor.h"
#include "../sensors/mhz19b.h"
#include "../sensors/npk_sensor.h"

#define TAG "SENSOR_MANAGER"


/* ==================================================
 * Initialize all sensors
 * ================================================== */

esp_err_t sensor_manager_init(
    i2c_master_bus_handle_t i2c_bus,
    i2c_master_bus_handle_t bh1750_bus
)
{
    esp_err_t ret;
    esp_err_t overall_status = ESP_OK;


    /* ----------------------------------------------
     * Shared ADC
     * ---------------------------------------------- */

    ret = adc_manager_init();

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "ADC manager initialization failed: %s",
            esp_err_to_name(ret)
        );

        overall_status = ret;
    }


    /* ----------------------------------------------
     * SHT40 - Temperature and humidity
     * I2C: SDA GPIO8, SCL GPIO9
     * ---------------------------------------------- */

    ret = sht40_init(i2c_bus);

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "SHT40 initialization failed: %s",
            esp_err_to_name(ret)
        );

        overall_status = ret;
    }


    /* ----------------------------------------------
     * BH1750 - Ambient light
     * I2C: SDA GPIO11, SCL GPIO12
     * Separate I2C bus
     * ---------------------------------------------- */

    ret = bh1750_init(bh1750_bus);

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "BH1750 initialization failed: %s",
            esp_err_to_name(ret)
        );

        overall_status = ret;
    }


    /* ----------------------------------------------
     * ML8511 - UV sensor
     * ---------------------------------------------- */

    ret = ml8511_init();

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "ML8511 initialization failed: %s",
            esp_err_to_name(ret)
        );

        overall_status = ret;
    }


    /* ----------------------------------------------
     * Voltage sensor
     * ---------------------------------------------- */

    ret = voltage_sensor_init();

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Voltage sensor initialization failed: %s",
            esp_err_to_name(ret)
        );

        overall_status = ret;
    }


    /* ----------------------------------------------
     * MH-Z19B - CO2 using PWM
     * ---------------------------------------------- */

    mhz19b_init();


    /* ----------------------------------------------
     * NPK soil sensor - Modbus RTU over RS485
     * ---------------------------------------------- */

    ret = npk_sensor_init();

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "NPK sensor initialization failed: %s",
            esp_err_to_name(ret)
        );

        overall_status = ret;
    }


    /* ----------------------------------------------
     * Initialization complete
     * ---------------------------------------------- */

    ESP_LOGI(
        TAG,
        "Sensor initialization complete"
    );

    return overall_status;
}


/* ==================================================
 * Read all sensors
 * ================================================== */

esp_err_t sensor_manager_read(
    sensor_data_t *data
)
{
    if (data == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret;


    /* ----------------------------------------------
     * SHT40 - Temperature and humidity
     * ---------------------------------------------- */

    ret = sht40_read(
        &data->temperature,
        &data->humidity
    );

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "SHT40 read failed: %s",
            esp_err_to_name(ret)
        );
    }


    /* ----------------------------------------------
     * BH1750 - Ambient light
     * ---------------------------------------------- */

    ret = bh1750_read(
        &data->light_lux
    );

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "BH1750 read failed: %s",
            esp_err_to_name(ret)
        );
    }


    /* ----------------------------------------------
     * ML8511 - UV sensor
     * ---------------------------------------------- */

    ret = ml8511_read(
        &data->uv_index,
        &data->uv_voltage
    );

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "ML8511 read failed: %s",
            esp_err_to_name(ret)
        );
    }


    /* ----------------------------------------------
     * Voltage sensor
     * ---------------------------------------------- */

    ret = voltage_sensor_read(
        &data->input_voltage
    );

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Voltage sensor read failed: %s",
            esp_err_to_name(ret)
        );
    }


    /* ----------------------------------------------
     * MH-Z19B - CO2
     * ---------------------------------------------- */

    data->co2_ppm = mhz19b_get_co2();


    /* ----------------------------------------------
     * NPK soil sensor
     * ---------------------------------------------- */

    npk_sensor_data_t npk_data = {0};

    ret = npk_sensor_read(&npk_data);

    if (ret == ESP_OK)
    {
        data->soil_moisture_percent =
            npk_data.moisture_percent;

        data->soil_temperature_c =
            npk_data.temperature_c;

        data->soil_conductivity_us_cm =
            npk_data.conductivity_us_cm;

        data->soil_ph =
            npk_data.ph;

        data->nitrogen_mg_kg =
            npk_data.nitrogen_mg_kg;

        data->phosphorus_mg_kg =
            npk_data.phosphorus_mg_kg;

        data->potassium_mg_kg =
            npk_data.potassium_mg_kg;
    }
    else
    {
        ESP_LOGE(
            TAG,
            "NPK sensor read failed: %s",
            esp_err_to_name(ret)
        );
    }


    /* ----------------------------------------------
     * All sensor reads attempted
     * ---------------------------------------------- */

    return ESP_OK;
}

idf_component_register(
    SRCS
        "main.c"
        "led_warning.c"
        "adc_manager/adc_manager.c"
        "sensors/sht40.c"
        "sensors/bh1750.c"
        "sensors/ml8511.c"
        "sensors/voltage_sensor.c"
        "sensors/mhz19b.c"
        "sensors/npk_sensor.c"
        "sensor_manager/sensor_manager.c"

    INCLUDE_DIRS
        "."
        "adc_manager"
        "sensors"
        "sensor_manager"

    REQUIRES
        esp_adc
        esp_driver_gpio
        esp_driver_uart
        esp_driver_i2c
        esp_driver_rmt
        esp_timer
        led_strip
)

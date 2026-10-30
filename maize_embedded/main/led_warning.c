#include "led_warning.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "led_strip.h"
#include "esp_log.h"

#define LED_GPIO       48
#define LED_COUNT      1
#define LED_TASK_STACK 2048
#define LED_TASK_PRIO  3

static const char *TAG = "LED_WARNING";

static led_strip_handle_t led;
static volatile led_status_t current_status = LED_STATUS_OK;

static esp_err_t set_color(uint8_t red, uint8_t green, uint8_t blue)
{
    esp_err_t err = led_strip_set_pixel(led, 0, red, green, blue);

    if (err != ESP_OK) {
        return err;
    }

    return led_strip_refresh(led);
}

static void led_task(void *arg)
{
    (void)arg;

    while (1) {
        led_status_t status = current_status;

        if (status == LED_STATUS_OK) {
            set_color(0, 0, 0);
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        /*
         * The error number determines the number of red blinks.
         * Each blink consists of 150 ms ON and 150 ms OFF.
         */
        unsigned int blinks = (unsigned int)status;

        for (unsigned int i = 0; i < blinks; i++) {
            set_color(255, 0, 0);
            vTaskDelay(pdMS_TO_TICKS(150));

            set_color(0, 0, 0);
            vTaskDelay(pdMS_TO_TICKS(150));
        }

        /* Pause before repeating the warning pattern. */
        vTaskDelay(pdMS_TO_TICKS(800));
    }
}

esp_err_t led_warning_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = LED_COUNT,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 64,
        .flags.with_dma = false,
    };

    esp_err_t err = led_strip_new_rmt_device(
        &strip_config,
        &rmt_config,
        &led
    );

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "RGB LED initialization failed: %s",
                 esp_err_to_name(err));
        return err;
    }

    set_color(0, 0, 0);

    if (xTaskCreate(
            led_task,
            "led_warning",
            LED_TASK_STACK,
            NULL,
            LED_TASK_PRIO,
            NULL
        ) != pdPASS) {
        ESP_LOGE(TAG, "Could not create LED warning task");
        set_color(0, 0, 0);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "RGB LED initialized on GPIO %d", LED_GPIO);
    return ESP_OK;
}

void led_warning_set_status(led_status_t status)
{
    current_status = status;
}

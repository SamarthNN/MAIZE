#include "mhz19b.h"

#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

#define MHZ19B_PWM_PIN GPIO_NUM_18

static const char *TAG = "MHZ19B";

/*
 * Timing information
 *
 * rising_time:
 * Time of most recent rising edge.
 *
 * high_time:
 * Duration of the HIGH portion of PWM.
 *
 * period_time:
 * Duration between two rising edges.
 */
static volatile int64_t rising_time = 0;
static volatile int64_t high_time = 0;
static volatile int64_t period_time = 0;

/*
 * Calculated values
 */
static volatile uint16_t co2_ppm = 0;
static volatile uint32_t duty_x100 = 0;


/* =========================================================
 * PWM ISR
 * ========================================================= */

static void IRAM_ATTR mhz19b_pwm_isr(void *arg)
{
    int level = gpio_get_level(MHZ19B_PWM_PIN);

    int64_t now = esp_timer_get_time();

    /*
     * Rising edge
     */
    if (level == 1)
    {
        /*
         * If this is not the first rising edge,
         * calculate the PWM period.
         */
        if (rising_time != 0)
        {
            int64_t new_period = now - rising_time;

            /*
             * Only accept a sensible period.
             *
             * MH-Z19B PWM is approximately 1 second.
             */
            if (new_period > 500000 &&
                new_period < 3000000)
            {
                period_time = new_period;

                /*
                 * We now have:
                 *
                 * previous HIGH time
                 * current PERIOD
                 *
                 * Calculate CO2 only if HIGH is valid.
                 */
                if (high_time > 0 &&
                    high_time <= new_period)
                {
                    /*
                     * CO2:
                     *
                     * CO2 = 2000 × HIGH / PERIOD
                     *
                     * Use integer arithmetic here.
                     * No floating point inside ISR.
                     */

                    int64_t ppm =
                        (high_time * 2000) / new_period;

                    if (ppm <= 2000)
                    {
                        co2_ppm = (uint16_t)ppm;
                    }

                    /*
                     * Duty × 100
                     *
                     * Example:
                     *
                     * 39.54% → 3954
                     */
                    duty_x100 =
                        (uint32_t)(
                            (high_time * 10000) /
                            new_period
                        );
                }
            }
        }

        /*
         * Start timing the next PWM cycle.
         */
        rising_time = now;
    }

    /*
     * Falling edge
     */
    else
    {
        /*
         * HIGH duration.
         *
         * Do NOT calculate CO2 here because
         * we don't necessarily have a valid
         * period yet.
         */
        if (rising_time != 0)
        {
            int64_t new_high = now - rising_time;

            /*
             * Sanity check.
             */
            if (new_high > 0 &&
                new_high < 3000000)
            {
                high_time = new_high;
            }
        }
    }
}


/* =========================================================
 * Initialization
 * ========================================================= */

void mhz19b_init(void)
{
    ESP_LOGI(
        TAG,
        "Initializing MH-Z19B PWM on GPIO%d",
        MHZ19B_PWM_PIN
    );

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << MHZ19B_PWM_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };

    ESP_ERROR_CHECK(
        gpio_config(&io_conf)
    );

    ESP_ERROR_CHECK(
        gpio_install_isr_service(
            ESP_INTR_FLAG_IRAM
        )
    );

    ESP_ERROR_CHECK(
        gpio_isr_handler_add(
            MHZ19B_PWM_PIN,
            mhz19b_pwm_isr,
            NULL
        )
    );

    ESP_LOGI(
        TAG,
        "MH-Z19B PWM initialized successfully"
    );
}


/* =========================================================
 * Get CO2
 * ========================================================= */

uint16_t mhz19b_get_co2(void)
{
    return co2_ppm;
}


/* =========================================================
 * Get Duty Cycle
 * ========================================================= */

float mhz19b_get_duty(void)
{
    return ((float)duty_x100 / 100.0f);
}


/* =========================================================
 * Get PWM Period
 * ========================================================= */

uint32_t mhz19b_get_period_us(void)
{
    int64_t period = period_time;

    if (period <= 0)
    {
        return 0;
    }

    return (uint32_t)period;
}


/* =========================================================
 * Get PWM HIGH Time
 * ========================================================= */

uint32_t mhz19b_get_high_time_us(void)
{
    int64_t high = high_time;

    if (high <= 0)
    {
        return 0;
    }

    return (uint32_t)high;
}

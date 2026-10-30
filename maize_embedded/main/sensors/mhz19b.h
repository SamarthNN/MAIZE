#ifndef MHZ19B_H
#define MHZ19B_H

#include <stdint.h>

/**
 * Initialize MH-Z19B PWM input.
 *
 * PWM pin:
 * ESP32-S3 GPIO18
 */
void mhz19b_init(void);

/**
 * Get the latest CO2 concentration.
 *
 * Returns:
 *   CO2 concentration in ppm
 *
 * Returns 0 if no valid PWM measurement
 * has been received yet.
 */
uint16_t mhz19b_get_co2(void);

/**
 * Get the latest PWM duty cycle.
 *
 * Returns:
 *   Duty cycle in percent.
 */
float mhz19b_get_duty(void);

/**
 * Get the PWM period in microseconds.
 */
uint32_t mhz19b_get_period_us(void);

/**
 * Get the PWM HIGH time in microseconds.
 */
uint32_t mhz19b_get_high_time_us(void);

#endif // MHZ19B_H

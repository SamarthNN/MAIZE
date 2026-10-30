#ifndef ML8511_H
#define ML8511_H

#include "esp_err.h"

/**
 * Initialize ML8511 ADC.
 *
 * @return ESP_OK on success.
 */
esp_err_t ml8511_init(void);

/**
 * Read ML8511 UV sensor output.
 *
 * @param uv_index Pointer to store estimated UV intensity
 *                 in mW/cm^2 (not standardized UV Index).
 * @param voltage  Pointer to store sensor output voltage in volts.
 *
 * @return ESP_OK on success.
 */
esp_err_t ml8511_read(
    float *uv_index,
    float *voltage
);

#endif

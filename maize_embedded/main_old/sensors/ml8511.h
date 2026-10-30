#ifndef ML8511_H
#define ML8511_H

#include "esp_err.h"

/**
 * Initialize ML8511 ADC.
 */
esp_err_t ml8511_init(void);

/**
 * Read UV sensor output.
 *
 * @param uv_index Pointer to store UV index.
 * @param voltage Pointer to store sensor output voltage.
 *
 * @return ESP_OK on success.
 */
esp_err_t ml8511_read(
    float *uv_index,
    float *voltage
);

#endif

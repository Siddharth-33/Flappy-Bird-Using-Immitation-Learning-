// ============================================================
// Touch Driver — XPT2046 Resistive Touch Controller
// Shares SPI bus with ILI9341 display, separate chip select.
// ============================================================
#pragma once

#include <stdbool.h>
#include <stdint.h>

// ---- Touch Pin Definitions ----
#define PIN_TOUCH_CS   15  // Touch Chip Select (T_CS)
#define PIN_TOUCH_IRQ  16  // Touch Interrupt (T_IRQ) - Active LOW

/**
 * @brief Initialize touch controller GPIO and SPI device.
 *        Must be called AFTER display_init() since they share SPI2_HOST.
 */
void touch_init(void);

/**
 * @brief Check if the screen is currently being pressed.
 * @return true if touched, false otherwise
 */
bool touch_is_pressed(void);

/**
 * @brief Read 12-bit raw touch coordinates.
 * @param x  Pointer to store raw X value
 * @param y  Pointer to store raw Y value
 * @return true if valid touch data was read
 */
bool touch_read_raw(uint16_t *x, uint16_t *y);

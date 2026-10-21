// ============================================================
// Display Driver — 2.8" ILI9341 SPI TFT (320x240 landscape)
// Uses ESP-IDF esp_lcd component with SPI DMA
// ============================================================
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "game_engine.h"

// ---- Pin Definitions (ESP32-S3 -> ILI9341) ----
#define PIN_LCD_MOSI   11
#define PIN_LCD_SCLK   12
#define PIN_LCD_MISO   13
#define PIN_LCD_CS     10
#define PIN_LCD_DC     9
#define PIN_LCD_RST    8
#define PIN_LCD_BL     14

// ---- RGB565 Color Definitions ----
#define COLOR_SKY       0x65BD   // Light sky blue
#define COLOR_BIRD      0xFFA0   // Yellow/orange bird
#define COLOR_PIPE      0x3666   // Dark green pipes
#define COLOR_PIPE_LIP  0x5ECB   // Lighter green pipe lip/edge
#define COLOR_GROUND    0xDEB8   // Sandy ground
#define COLOR_WHITE     0xFFFF
#define COLOR_BLACK     0x0000
#define COLOR_RED       0xF800
#define COLOR_DARK_GRAY 0x18C3

/**
 * @brief Initialize the SPI bus, ILI9341 panel, and backlight.
 */
void display_init(void);

/**
 * @brief Render one complete frame of the Flappy Bird game.
 *
 * @param gs          Current game state
 * @param ai_action   Last AI action string ("FLAP" or "GLIDE")
 * @param is_ai_mode  true if AI is playing, false if human
 * @param inference_us Inference time in microseconds
 */
void display_render_frame(const game_state_t *gs, const char *ai_action,
                          bool is_ai_mode, uint32_t inference_us);

/**
 * @brief Show the Game Over splash screen with final score.
 */
void display_game_over(int score);

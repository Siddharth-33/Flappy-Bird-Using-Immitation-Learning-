// ============================================================
// Flappy Bird DAgger AI — ESP32-S3 Main Application
//
// Dual-Core FreeRTOS Architecture:
// - Core 0 (PRO_CPU): Game Physics + Neural Network Inference (DAgger)
// - Core 1 (APP_CPU): ILI9341 2.8" SPI Display DMA Rendering
//
// Features:
// - Real-time AI play using imitation learning (DAgger)
// - Mode switching: AI Mode vs. Human Touch Play
// - Microsecond inference latency benchmarking on screen
// ============================================================

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "display_driver.h"
#include "touch_driver.h"
#include "game_engine.h"
#include "neural_network.h"

static const char *TAG = "flappy_dagger_app";

// ---- Shared State (Protected by FreeRTOS Mutex) ----
static game_state_t g_game_state;
static SemaphoreHandle_t g_state_mutex = NULL;
static bool g_ai_mode = true;
static const char *g_ai_action = "GLIDE";
static uint32_t g_inference_us = 0;

#define FRAME_PERIOD_MS 33 // ~30 FPS

// ============================================================
// Core 0 Task: Game Engine & Neural Network Inference
// ============================================================
static void game_logic_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Game & DAgger AI task running on Core %d", xPortGetCoreID());

    game_init(&g_game_state);

    bool prev_touch = false;
    int touch_hold_counter = 0;

    while (1) {
        bool touched = touch_is_pressed();

        // 1. Long press (>1.2 seconds) toggles between AI Mode and Human Mode
        if (touched) {
            touch_hold_counter++;
            if (touch_hold_counter == 36) { // ~1.2s at 33ms per loop
                g_ai_mode = !g_ai_mode;
                ESP_LOGI(TAG, "Mode toggled! AI Mode: %s", g_ai_mode ? "ENABLED" : "DISABLED");
            }
        } else {
            touch_hold_counter = 0;
        }

        // 2. Game-Over Restart on Tap
        if (g_game_state.game_over) {
            if (touched && !prev_touch) {
                xSemaphoreTake(g_state_mutex, portMAX_DELAY);
                game_init(&g_game_state);
                xSemaphoreGive(g_state_mutex);
                vTaskDelay(pdMS_TO_TICKS(250)); // Debounce
            }
            prev_touch = touched;
            vTaskDelay(pdMS_TO_TICKS(FRAME_PERIOD_MS));
            continue;
        }

        // 3. Action Selection (AI vs Human)
        bool flap = false;

        if (g_ai_mode) {
            // Extract the 8 normalized state features matching Python PLE
            float state[8];
            xSemaphoreTake(g_state_mutex, portMAX_DELAY);
            game_get_nn_state(&g_game_state, state);
            xSemaphoreGive(g_state_mutex);

            // Benchmark neural network forward pass in microseconds
            int64_t t0 = esp_timer_get_time();
            int action = nn_predict(state);
            int64_t t1 = esp_timer_get_time();
            g_inference_us = (uint32_t)(t1 - t0);

            flap = (action == 1);
            g_ai_action = flap ? "FLAP" : "GLIDE";
        } else {
            // Human player: tap screen to flap
            if (touched && !prev_touch) {
                flap = true;
            }
            g_ai_action = flap ? "FLAP" : "GLIDE";
            g_inference_us = 0;
        }

        prev_touch = touched;

        // 4. Update Game Physics
        xSemaphoreTake(g_state_mutex, portMAX_DELAY);
        game_update(&g_game_state, flap);
        xSemaphoreGive(g_state_mutex);

        vTaskDelay(pdMS_TO_TICKS(FRAME_PERIOD_MS));
    }
}

// ============================================================
// Core 1 Task: High-Speed Display DMA Rendering
// ============================================================
static void render_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Display render task running on Core %d", xPortGetCoreID());

    while (1) {
        // Take a clean snapshot of game state
        game_state_t snapshot;
        bool is_ai;
        const char *action;
        uint32_t inf_us;

        xSemaphoreTake(g_state_mutex, portMAX_DELAY);
        snapshot = g_game_state;
        is_ai = g_ai_mode;
        action = g_ai_action;
        inf_us = g_inference_us;
        xSemaphoreGive(g_state_mutex);

        if (snapshot.game_over) {
            display_game_over(snapshot.score);
        } else {
            display_render_frame(&snapshot, action, is_ai, inf_us);
        }

        vTaskDelay(pdMS_TO_TICKS(FRAME_PERIOD_MS));
    }
}

// ============================================================
// Application Entry Point
// ============================================================
void app_main(void)
{
    ESP_LOGI(TAG, "=================================================");
    ESP_LOGI(TAG, "  ESP32-S3 Flappy Bird DAgger Imitation Learning ");
    ESP_LOGI(TAG, "  Microprocessor & Microcontrollers Project      ");
    ESP_LOGI(TAG, "=================================================");

    // 1. Initialize Hardware Peripherals
    display_init(); // SPI bus & ILI9341 LCD
    touch_init();   // XPT2046 Touch Controller
    nn_init();      // Neural Network (weights in flash)

    // 2. Create Synchronization Mutex
    g_state_mutex = xSemaphoreCreateMutex();
    assert(g_state_mutex != NULL);

    // 3. Pin Tasks to Separate CPU Cores
    // Core 0: Game Physics & Neural Network Inference
    xTaskCreatePinnedToCore(game_logic_task, "game_logic_task", 4096, NULL, 5, NULL, 0);

    // Core 1: Display DMA Rendering
    xTaskCreatePinnedToCore(render_task, "render_task", 4096, NULL, 4, NULL, 1);

    ESP_LOGI(TAG, "Dual-core tasks launched. Ready to play!");
}

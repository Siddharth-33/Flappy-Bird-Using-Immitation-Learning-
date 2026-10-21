// ============================================================
// Display Driver — 2.8" ILI9341 SPI TFT (320x240 landscape)
// Uses direct ESP-IDF SPI Master with DMA for high-speed,
// zero-dependency rendering across all ESP-IDF v5.x versions.
// ============================================================

#include "display_driver.h"
#include "game_engine.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "display_ili9341";

static spi_device_handle_t spi_lcd = NULL;

// Buffer: 1 scanline (320 pixels * 2 bytes = 640 bytes)
// Two buffers for ping-pong DMA transfer
#define LINE_BUF_SIZE (SCREEN_WIDTH * sizeof(uint16_t))
static uint16_t *line_buf[2] = {NULL, NULL};
static int cur_buf = 0;

// ---- Low-Level SPI Transmission ----

static void lcd_cmd(uint8_t cmd)
{
    gpio_set_level(PIN_LCD_DC, 0); // DC = 0 for command
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.tx_buffer = &cmd;
    spi_device_polling_transmit(spi_lcd, &t);
}

static void lcd_data(const uint8_t *data, int len)
{
    if (len <= 0) return;
    gpio_set_level(PIN_LCD_DC, 1); // DC = 1 for data
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = len * 8;
    t.tx_buffer = data;
    spi_device_polling_transmit(spi_lcd, &t);
}

static void lcd_data_byte(uint8_t val)
{
    lcd_data(&val, 1);
}

static void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t data[4];

    // Column Address Set (0x2A)
    lcd_cmd(0x2A);
    data[0] = (x0 >> 8) & 0xFF;
    data[1] = x0 & 0xFF;
    data[2] = (x1 >> 8) & 0xFF;
    data[3] = x1 & 0xFF;
    lcd_data(data, 4);

    // Page Address Set (0x2B)
    lcd_cmd(0x2B);
    data[0] = (y0 >> 8) & 0xFF;
    data[1] = y0 & 0xFF;
    data[2] = (y1 >> 8) & 0xFF;
    data[3] = y1 & 0xFF;
    lcd_data(data, 4);

    // Memory Write (0x2C)
    lcd_cmd(0x2C);
}

static void lcd_send_line_dma(uint16_t *pixels, int len_pixels)
{
    gpio_set_level(PIN_LCD_DC, 1); // Data
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = len_pixels * 16;
    t.tx_buffer = pixels;
    spi_device_polling_transmit(spi_lcd, &t);
}

// Swap bytes for ILI9341 big-endian RGB565
static inline uint16_t swap16(uint16_t c)
{
    return (c >> 8) | (c << 8);
}

// ---- Simple 5x7 ASCII Font for Game HUD ----
static const uint8_t font_5x7[][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A (10)
    {0x00, 0x00, 0x00, 0x00, 0x00}, // ' ' (11)
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C (12)
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D (13)
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E (14)
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // F (15)
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G (16)
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H (17)
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I (18)
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L (19)
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M (20)
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O (21)
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P (22)
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R (23)
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S (24)
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T (25)
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U (26)
    {0x7F, 0x20, 0x18, 0x20, 0x7F}, // W (27)
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K (28)
    {0x00, 0x36, 0x36, 0x00, 0x00}, // : (29)
};

static int char_to_font_index(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    switch (c) {
        case 'A': return 10;
        case ' ': return 11;
        case 'C': return 12;
        case 'D': return 13;
        case 'E': return 14;
        case 'F': return 15;
        case 'G': return 16;
        case 'H': return 17;
        case 'I': return 18;
        case 'L': return 19;
        case 'M': return 20;
        case 'O': return 21;
        case 'P': return 22;
        case 'R': return 23;
        case 'S': return 24;
        case 'T': return 25;
        case 'U': return 26;
        case 'W': return 27;
        case 'K': return 28;
        case ':': return 29;
        default:  return 11;
    }
}

static void draw_char_to_line(uint16_t *buf, int screen_y, char c,
                              int char_x, int char_y, int scale, uint16_t color)
{
    int idx = char_to_font_index(c);
    int font_row = (screen_y - char_y) / scale;
    if (font_row < 0 || font_row >= 7) return;

    for (int col = 0; col < 5; col++) {
        if (font_5x7[idx][col] & (1 << font_row)) {
            for (int s = 0; s < scale; s++) {
                int px = char_x + col * scale + s;
                if (px >= 0 && px < SCREEN_WIDTH) {
                    buf[px] = color;
                }
            }
        }
    }
}

static void draw_string_to_line(uint16_t *buf, int screen_y, const char *str,
                                int start_x, int start_y, int scale, uint16_t color)
{
    int x = start_x;
    for (int i = 0; str[i]; i++) {
        draw_char_to_line(buf, screen_y, str[i], x, start_y, scale, color);
        x += 6 * scale;
    }
}

// ---- Public Functions ----

void display_init(void)
{
    ESP_LOGI(TAG, "Initializing ILI9341 SPI Display...");

    // 1. Configure DC, RST, and BL GPIOs
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_LCD_DC) | (1ULL << PIN_LCD_RST) | (1ULL << PIN_LCD_BL),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    gpio_set_level(PIN_LCD_BL, 1); // Turn on backlight

    // 2. Initialize SPI Bus (SPI2_HOST)
    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_LCD_MISO,
        .mosi_io_num = PIN_LCD_MOSI,
        .sclk_io_num = PIN_LCD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LINE_BUF_SIZE * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // 3. Add ILI9341 Device to SPI Bus (40 MHz SPI Clock)
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 40 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PIN_LCD_CS,
        .queue_size = 7,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &spi_lcd));

    // 4. Hardware Reset
    gpio_set_level(PIN_LCD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    // 5. Send ILI9341 Initialization Sequence
    lcd_cmd(0x01); // Software Reset
    vTaskDelay(pdMS_TO_TICKS(100));

    lcd_cmd(0x28); // Display OFF

    // Power Control A & B
    lcd_cmd(0xCB);
    uint8_t cb_data[] = {0x39, 0x2C, 0x00, 0x34, 0x02};
    lcd_data(cb_data, 5);

    lcd_cmd(0xCF);
    uint8_t cf_data[] = {0x00, 0xC1, 0x30};
    lcd_data(cf_data, 3);

    lcd_cmd(0xE8);
    uint8_t e8_data[] = {0x85, 0x00, 0x78};
    lcd_data(e8_data, 3);

    lcd_cmd(0xEA);
    uint8_t ea_data[] = {0x00, 0x00};
    lcd_data(ea_data, 2);

    lcd_cmd(0xED);
    uint8_t ed_data[] = {0x64, 0x03, 0x12, 0x81};
    lcd_data(ed_data, 4);

    lcd_cmd(0xF7);
    lcd_data_byte(0x20);

    // Power Control 1 & 2
    lcd_cmd(0xC0);
    lcd_data_byte(0x23);

    lcd_cmd(0xC1);
    lcd_data_byte(0x10);

    // VCOM Control 1 & 2
    lcd_cmd(0xC5);
    uint8_t c5_data[] = {0x3E, 0x28};
    lcd_data(c5_data, 2);

    lcd_cmd(0xC7);
    lcd_data_byte(0x86);

    // Memory Access Control (Orientation: Landscape 320x240)
    // 0x28 = BGR + Row/Col exchange (Landscape)
    lcd_cmd(0x36);
    lcd_data_byte(0x28);

    // Pixel Format (16 bits/pixel: RGB565)
    lcd_cmd(0x3A);
    lcd_data_byte(0x55);

    // Frame Rate Control
    lcd_cmd(0xB1);
    uint8_t b1_data[] = {0x00, 0x18};
    lcd_data(b1_data, 2);

    // Display Function Control
    lcd_cmd(0xB6);
    uint8_t b6_data[] = {0x08, 0x82, 0x27};
    lcd_data(b6_data, 3);

    // Sleep Out
    lcd_cmd(0x11);
    vTaskDelay(pdMS_TO_TICKS(120));

    // Display ON
    lcd_cmd(0x29);
    vTaskDelay(pdMS_TO_TICKS(20));

    // Allocate DMA-capable line buffers
    for (int i = 0; i < 2; i++) {
        line_buf[i] = heap_caps_malloc(LINE_BUF_SIZE, MALLOC_CAP_DMA);
        if (!line_buf[i]) {
            ESP_LOGE(TAG, "Failed to allocate DMA line buffer %d", i);
        }
    }

    ESP_LOGI(TAG, "ILI9341 display initialized successfully (320x240 Landscape)");
}

void display_render_frame(const game_state_t *gs, const char *ai_action,
                          bool is_ai_mode, uint32_t inference_us)
{
    // Convert bird coordinates from game space to screen space
    int bird_sx = game_to_screen_x(BIRD_X);
    int bird_sy = game_to_screen_y(gs->bird.y);
    int bird_sw = game_to_screen_x(BIRD_X + BIRD_WIDTH) - bird_sx;
    int bird_sh = game_to_screen_y(gs->bird.y + BIRD_HEIGHT) - bird_sy;
    if (bird_sw < 8) bird_sw = 8;
    if (bird_sh < 8) bird_sh = 8;

    int ground_y = SCREEN_HEIGHT - 16;

    char score_str[16];
    snprintf(score_str, sizeof(score_str), "%d", gs->score);

    char status_str[32];
    if (is_ai_mode) {
        snprintf(status_str, sizeof(status_str), "AI %s %luUS",
                 ai_action ? ai_action : "", (unsigned long)inference_us);
    } else {
        snprintf(status_str, sizeof(status_str), "HUMAN MODE");
    }

    // Set drawing window to whole screen
    lcd_set_window(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);

    // Render line-by-line for memory efficiency
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        uint16_t *buf = line_buf[cur_buf];

        // 1. Background
        uint16_t bg_color = (y < ground_y) ? COLOR_SKY : COLOR_GROUND;
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            buf[x] = swap16(bg_color);
        }

        // 2. Pipes
        for (int i = 0; i < MAX_PIPES; i++) {
            if (!gs->pipes[i].active) continue;

            int pipe_sx = game_to_screen_x(gs->pipes[i].x);
            int pipe_sw = game_to_screen_x(gs->pipes[i].x + PIPE_WIDTH) - pipe_sx;
            int gap_top_sy = game_to_screen_y(gs->pipes[i].top_y);
            int gap_bot_sy = game_to_screen_y(gs->pipes[i].bottom_y);

            if (pipe_sx + pipe_sw < 0 || pipe_sx >= SCREEN_WIDTH) continue;

            int x_start = (pipe_sx > 0) ? pipe_sx : 0;
            int x_end = (pipe_sx + pipe_sw < SCREEN_WIDTH) ? pipe_sx + pipe_sw : SCREEN_WIDTH;

            // Top pipe
            if (y < gap_top_sy && y < ground_y) {
                bool is_lip = (y >= gap_top_sy - 4 && y < gap_top_sy);
                uint16_t pcolor = is_lip ? COLOR_PIPE_LIP : COLOR_PIPE;
                for (int x = x_start; x < x_end; x++) {
                    buf[x] = swap16(pcolor);
                }
            }

            // Bottom pipe
            if (y >= gap_bot_sy && y < ground_y) {
                bool is_lip = (y >= gap_bot_sy && y < gap_bot_sy + 4);
                uint16_t pcolor = is_lip ? COLOR_PIPE_LIP : COLOR_PIPE;
                for (int x = x_start; x < x_end; x++) {
                    buf[x] = swap16(pcolor);
                }
            }
        }

        // 3. Bird
        if (y >= bird_sy && y < bird_sy + bird_sh) {
            for (int x = bird_sx; x < bird_sx + bird_sw && x < SCREEN_WIDTH; x++) {
                if (x < 0) continue;
                bool is_border = (y == bird_sy || y == bird_sy + bird_sh - 1 ||
                                  x == bird_sx || x == bird_sx + bird_sw - 1);
                buf[x] = swap16(is_border ? COLOR_BLACK : COLOR_BIRD);
            }
        }

        // 4. Score at top-center
        int score_px_width = (int)strlen(score_str) * 6 * 3;
        int score_x = (SCREEN_WIDTH - score_px_width) / 2;
        draw_string_to_line(buf, y, score_str, score_x, 8, 3, swap16(COLOR_WHITE));

        // 5. AI Status at top-left
        draw_string_to_line(buf, y, status_str, 6, 6, 1, swap16(COLOR_BLACK));

        // Send line via DMA
        lcd_send_line_dma(buf, SCREEN_WIDTH);
        cur_buf = 1 - cur_buf;
    }
}

void display_game_over(int score)
{
    char score_str[32];
    snprintf(score_str, sizeof(score_str), "SCORE: %d", score);

    lcd_set_window(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);

    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        uint16_t *buf = line_buf[cur_buf];

        for (int x = 0; x < SCREEN_WIDTH; x++) {
            buf[x] = swap16(COLOR_DARK_GRAY);
        }

        // GAME OVER
        draw_string_to_line(buf, y, "PATIL POWER", 70, 75, 3, swap16(COLOR_RED));

        // SCORE: X
        int sp_width = (int)strlen(score_str) * 6 * 2;
        draw_string_to_line(buf, y, score_str, (SCREEN_WIDTH - sp_width) / 2, 125, 2, swap16(COLOR_WHITE));

        // TOUCH TO RESTART
        draw_string_to_line(buf, y, "TOUCH TO RESTART", 32, 175, 2, swap16(COLOR_SKY));

        lcd_send_line_dma(buf, SCREEN_WIDTH);
        cur_buf = 1 - cur_buf;
    }
}

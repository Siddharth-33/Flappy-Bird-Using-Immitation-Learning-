// ============================================================
// Touch Driver — XPT2046 Resistive Touch Controller
// Uses SPI to read touch state and coordinates.
// ============================================================

#include "touch_driver.h"
#include "display_driver.h"
#include <string.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "touch_xpt2046";
static spi_device_handle_t spi_touch = NULL;

void touch_init(void)
{
    ESP_LOGI(TAG, "Initializing XPT2046 Touch Controller...");

    // 1. Configure T_IRQ pin (Input with pull-up, active LOW)
    gpio_config_t irq_cfg = {
        .pin_bit_mask = (1ULL << PIN_TOUCH_IRQ),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&irq_cfg);

    // 2. Add XPT2046 to existing SPI bus (SPI2_HOST)
    // XPT2046 works at max 2-2.5 MHz
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 2 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PIN_TOUCH_CS,
        .queue_size = 3,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &spi_touch));

    ESP_LOGI(TAG, "Touch initialized on SPI2 (CS: GPIO %d, IRQ: GPIO %d)",
             PIN_TOUCH_CS, PIN_TOUCH_IRQ);
}

bool touch_is_pressed(void)
{
    // T_IRQ goes LOW when screen is physically pressed
    return (gpio_get_level(PIN_TOUCH_IRQ) == 0);
}

bool touch_read_raw(uint16_t *x, uint16_t *y)
{
    if (!touch_is_pressed()) {
        return false;
    }

    // XPT2046 12-bit differential read commands:
    // 0xD0 = Read X coordinate
    // 0x90 = Read Y coordinate
    uint8_t tx_x[3] = {0xD0, 0x00, 0x00};
    uint8_t rx_x[3] = {0};
    spi_transaction_t t_x;
    memset(&t_x, 0, sizeof(t_x));
    t_x.length = 24;
    t_x.tx_buffer = tx_x;
    t_x.rx_buffer = rx_x;
    spi_device_polling_transmit(spi_touch, &t_x);
    *x = ((rx_x[1] << 8) | rx_x[2]) >> 3;

    uint8_t tx_y[3] = {0x90, 0x00, 0x00};
    uint8_t rx_y[3] = {0};
    spi_transaction_t t_y;
    memset(&t_y, 0, sizeof(t_y));
    t_y.length = 24;
    t_y.tx_buffer = tx_y;
    t_y.rx_buffer = rx_y;
    spi_device_polling_transmit(spi_touch, &t_y);
    *y = ((rx_y[1] << 8) | rx_y[2]) >> 3;

    return true;
}

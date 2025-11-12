#ifndef GPIO_PTT_H
#define GPIO_PTT_H

#include <stdbool.h>

// GPIO pin definitions
#define GPIO_PTT_PIN        78      // Push-to-talk button
#define GPIO_LED_TX_PIN     79      // Transmit LED
#define GPIO_LED_RX_PIN     80      // Receive LED

// GPIO context
typedef struct {
    int ptt_fd;
    int led_tx_fd;
    int led_rx_fd;
    bool initialized;
} gpio_ctx_t;

// Initialize GPIO (export pins, set directions)
int gpio_init(gpio_ctx_t *ctx);

// Read PTT button state
bool gpio_read_ptt(gpio_ctx_t *ctx);

// Control TX LED
void gpio_set_tx_led(gpio_ctx_t *ctx, bool on);

// Control RX LED
void gpio_set_rx_led(gpio_ctx_t *ctx, bool on);

// Turn off all LEDs
void gpio_leds_off(gpio_ctx_t *ctx);

// Cleanup GPIO
void gpio_cleanup(gpio_ctx_t *ctx);

#endif // GPIO_PTT_H
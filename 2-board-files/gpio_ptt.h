#ifndef GPIO_PTT_H
#define GPIO_PTT_H

#include <stdbool.h>

#define GPIO_PTT_PIN        78
#define GPIO_LED_TX_PIN     79
#define GPIO_LED_RX_PIN     80

typedef struct {
    int ptt_fd;
    int led_tx_fd;
    int led_rx_fd;
    bool initialized;
} gpio_ctx_t;

// Initialize GPIO
int gpio_init(gpio_ctx_t *ctx);

// Read PTT button
bool gpio_read_ptt(gpio_ctx_t *ctx);

// Control LEDs
void gpio_set_tx_led(gpio_ctx_t *ctx, bool on);
void gpio_set_rx_led(gpio_ctx_t *ctx, bool on);

// Cleanup
void gpio_cleanup(gpio_ctx_t *ctx);

#endif // GPIO_PTT_H
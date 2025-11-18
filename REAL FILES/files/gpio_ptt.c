#include "gpio_ptt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define GPIO_PATH "/sys/class/gpio"

// Helper function to export a GPIO pin
static int gpio_export(int pin) {
    char path[64];
    int fd;
    
    // Checks if the pin is already exporteed or not
    snprintf(path, sizeof(path), GPIO_PATH "/gpio%d", pin);
    if (access(path, F_OK) == 0) {
        return 0;  // Already exported
    }
    
    // Otherwise, write the pin number to the export file
    fd = open(GPIO_PATH "/export", O_WRONLY);
    if (fd < 0) {
        perror("gpio_export open");
        return -1;
    }
    
    char buf[8];
    int len = snprintf(buf, sizeof(buf), "%d", pin);
    if (write(fd, buf, len) < 0) {
        if (errno != EBUSY) {  // EBUSY means already exported
            perror("gpio_export write");
            close(fd);
            return -1;
        }
    }
    
    close(fd);

    // Small delay to allow sysfs to create the gpio directory
    usleep(100000);
    return 0;
}

// Helper function to set GPIO direction
static int gpio_set_direction(int pin, const char *direction) {
    char path[64];
    int fd;
    
    // /gpioXX/value is the file we need to read/write the pin value
    snprintf(path, sizeof(path), GPIO_PATH "/gpio%d/direction", pin);

    // Open the GPIO direction to write in or out
    fd = open(path, O_WRONLY);
    if (fd < 0) {
        perror("gpio_set_direction open");
        return -1;
    }
    
    if (write(fd, direction, strlen(direction)) < 0) {
        perror("gpio_set_direction write");
        close(fd);
        return -1;
    }
    
    close(fd);
    return 0;
}

// Helper function to return a file descriptor for GPIO value
static int gpio_open_value(int pin) {
    char path[64];

    snprintf(path, sizeof(path), GPIO_PATH "/gpio%d/value", pin);
    
    // Keeps the FD open for fast access instread of needing to open and close every single time
    int fd = open(path, O_RDWR);
    if (fd < 0) {
        perror("gpio_open_value");
        return -1;
    }
    
    return fd;
}

// Initialize GPIO
int gpio_init(gpio_ctx_t *ctx) {
    // Clear the context structure
    memset(ctx, 0, sizeof(gpio_ctx_t));
    
    // Export pins
    if (gpio_export(GPIO_PTT_PIN) < 0 ||
        gpio_export(GPIO_LED_TX_PIN) < 0 ||
        gpio_export(GPIO_LED_RX_PIN) < 0) {
        fprintf(stderr, "Failed to export GPIO pins\n");
        return -1;
    }
    
    // Set directions
    if (gpio_set_direction(GPIO_PTT_PIN, "in") < 0) {
        fprintf(stderr, "Failed to set PTT direction\n");
        return -1;
    }
    
    if (gpio_set_direction(GPIO_LED_TX_PIN, "out") < 0 ||
        gpio_set_direction(GPIO_LED_RX_PIN, "out") < 0) {
        fprintf(stderr, "Failed to set LED directions\n");
        return -1;
    }
    
    // Open value files
    ctx->ptt_fd = gpio_open_value(GPIO_PTT_PIN);
    ctx->led_tx_fd = gpio_open_value(GPIO_LED_TX_PIN);
    ctx->led_rx_fd = gpio_open_value(GPIO_LED_RX_PIN);
    
    if (ctx->ptt_fd < 0 || ctx->led_tx_fd < 0 || ctx->led_rx_fd < 0) {
        fprintf(stderr, "Failed to open GPIO value files\n");
        if (ctx->ptt_fd >= 0) close(ctx->ptt_fd);
        if (ctx->led_tx_fd >= 0) close(ctx->led_tx_fd);
        if (ctx->led_rx_fd >= 0) close(ctx->led_rx_fd);
        return -1;
    }
    
    // Turn off LEDs initially
    gpio_leds_off(ctx);
    
    ctx->initialized = true;
    
    printf("GPIO initialised:\n");
    printf("  PTT Button: GPIO %d\n", GPIO_PTT_PIN);
    printf("  TX LED:     GPIO %d\n", GPIO_LED_TX_PIN);
    printf("  RX LED:     GPIO %d\n", GPIO_LED_RX_PIN);
    
    return 0;
}

// Read PTT button state
bool gpio_read_ptt(gpio_ctx_t *ctx) {
    if (!ctx->initialized || ctx->ptt_fd < 0) {
        return false;
    }
    
    char buf[2];
    // Move the file pointer back to the start before reading because we didn't close the file
    lseek(ctx->ptt_fd, 0, SEEK_SET);
    
    // Now we can finally read one character to get the value
    if (read(ctx->ptt_fd, buf, 1) != 1) {
        return false;
    }
    
    return buf[0] == '1';
}

// Write to the GPIO value file
static int safe_write(int fd, const char *buf, size_t count) {
    ssize_t result = write(fd, buf, count);
    if (result < 0) {
        perror("write failed");
        return -1;
    }
    return 0;
}

// Control TX LED
void gpio_set_tx_led(gpio_ctx_t *ctx, bool on) {
    if (!ctx->initialized || ctx->led_tx_fd < 0) return;
    
    // Turn on and off the LED by writing '1' or '0' to the value file
    const char *value = on ? "1" : "0";
    if (safe_write(ctx->led_tx_fd, value, 1) < 0) {
        fprintf(stderr, "Failed to set TX LED\n");
    }
}

// Same thing really for RX LED
void gpio_set_rx_led(gpio_ctx_t *ctx, bool on) {
    if (!ctx->initialized || ctx->led_rx_fd < 0) return;
    
    const char *value = on ? "1" : "0";
    if (safe_write(ctx->led_rx_fd, value, 1) < 0) {
        fprintf(stderr, "Failed to set RX LED\n");
    }
}

// Turn off all LEDs
void gpio_leds_off(gpio_ctx_t *ctx) {
    gpio_set_tx_led(ctx, false);
    gpio_set_rx_led(ctx, false);
}

// Cleanup GPIO
void gpio_cleanup(gpio_ctx_t *ctx) {
    if (ctx->initialized) {
        gpio_leds_off(ctx);
        
        if (ctx->ptt_fd >= 0) close(ctx->ptt_fd);
        if (ctx->led_tx_fd >= 0) close(ctx->led_tx_fd);
        if (ctx->led_rx_fd >= 0) close(ctx->led_rx_fd);
        
        ctx->initialized = false;
    }
}
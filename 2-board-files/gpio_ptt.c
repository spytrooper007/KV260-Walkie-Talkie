#include "gpio_ptt.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define GPIO_PATH "/sys/class/gpio"

static int gpio_export(int pin) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%s/gpio%d", GPIO_PATH, pin);
    if (access(buf, F_OK) == 0) return 0;
    
    int fd = open(GPIO_PATH "/export", O_WRONLY);
    if (fd < 0) return -1;
    
    snprintf(buf, sizeof(buf), "%d", pin);
    write(fd, buf, strlen(buf));
    close(fd);
    usleep(100000);
    return 0;
}

static int gpio_set_dir(int pin, const char *dir) {
    char path[64];
    snprintf(path, sizeof(path), "%s/gpio%d/direction", GPIO_PATH, pin);
    
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    
    write(fd, dir, strlen(dir));
    close(fd);
    return 0;
}

static int gpio_open_val(int pin) {
    char path[64];
    snprintf(path, sizeof(path), "%s/gpio%d/value", GPIO_PATH, pin);
    return open(path, O_RDWR);
}

int gpio_init(gpio_ctx_t *ctx) {
    memset(ctx, 0, sizeof(gpio_ctx_t));
    
    if (gpio_export(GPIO_PTT_PIN) < 0 ||
        gpio_export(GPIO_LED_TX_PIN) < 0 ||
        gpio_export(GPIO_LED_RX_PIN) < 0) {
        return -1;
    }
    
    if (gpio_set_dir(GPIO_PTT_PIN, "in") < 0 ||
        gpio_set_dir(GPIO_LED_TX_PIN, "out") < 0 ||
        gpio_set_dir(GPIO_LED_RX_PIN, "out") < 0) {
        return -1;
    }
    
    ctx->ptt_fd = gpio_open_val(GPIO_PTT_PIN);
    ctx->led_tx_fd = gpio_open_val(GPIO_LED_TX_PIN);
    ctx->led_rx_fd = gpio_open_val(GPIO_LED_RX_PIN);
    
    if (ctx->ptt_fd < 0 || ctx->led_tx_fd < 0 || ctx->led_rx_fd < 0) {
        if (ctx->ptt_fd >= 0) close(ctx->ptt_fd);
        if (ctx->led_tx_fd >= 0) close(ctx->led_tx_fd);
        if (ctx->led_rx_fd >= 0) close(ctx->led_rx_fd);
        return -1;
    }
    
    gpio_set_tx_led(ctx, false);
    gpio_set_rx_led(ctx, false);
    
    ctx->initialized = true;
    printf("GPIO initialized\n");
    return 0;
}

bool gpio_read_ptt(gpio_ctx_t *ctx) {
    if (!ctx->initialized) return false;
    
    char c;
    lseek(ctx->ptt_fd, 0, SEEK_SET);
    if (read(ctx->ptt_fd, &c, 1) != 1) return false;
    return c == '1';
}

void gpio_set_tx_led(gpio_ctx_t *ctx, bool on) {
    if (!ctx->initialized) return;
    lseek(ctx->led_tx_fd, 0, SEEK_SET);
    write(ctx->led_tx_fd, on ? "1" : "0", 1);
}

void gpio_set_rx_led(gpio_ctx_t *ctx, bool on) {
    if (!ctx->initialized) return;
    lseek(ctx->led_rx_fd, 0, SEEK_SET);
    write(ctx->led_rx_fd, on ? "1" : "0", 1);
}

void gpio_cleanup(gpio_ctx_t *ctx) {
    if (ctx->initialized) {
        gpio_set_tx_led(ctx, false);
        gpio_set_rx_led(ctx, false);
        if (ctx->ptt_fd >= 0) close(ctx->ptt_fd);
        if (ctx->led_tx_fd >= 0) close(ctx->led_tx_fd);
        if (ctx->led_rx_fd >= 0) close(ctx->led_rx_fd);
        ctx->initialized = false;
    }
}
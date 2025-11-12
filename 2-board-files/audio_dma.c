#include "audio_dma.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

// DMA Register Offsets
#define MM2S_CTRL       0x00
#define MM2S_STATUS     0x04
#define MM2S_SA         0x18
#define MM2S_LENGTH     0x28
#define S2MM_CTRL       0x30
#define S2MM_STATUS     0x34
#define S2MM_DA         0x48
#define S2MM_LENGTH     0x58

// Control bits
#define CTRL_RUN        0x01
#define CTRL_RESET      0x04
#define STAT_IDLE       0x02

// Register access
#define DMA_WR(ctx, off, val) \
    (*((volatile uint32_t *)((ctx)->dma_regs + (off))) = (val))
#define DMA_RD(ctx, off) \
    (*((volatile uint32_t *)((ctx)->dma_regs + (off))))

// Initialize DMA
int dma_init(dma_ctx_t *ctx) {
    memset(ctx, 0, sizeof(dma_ctx_t));
    
    ctx->mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (ctx->mem_fd < 0) {
        perror("open /dev/mem");
        return -1;
    }
    
    // Map DMA registers
    ctx->dma_regs = mmap(NULL, 0x10000, PROT_READ | PROT_WRITE,
                        MAP_SHARED, ctx->mem_fd, DMA_BASE_ADDR);
    if (ctx->dma_regs == MAP_FAILED) {
        perror("mmap DMA regs");
        close(ctx->mem_fd);
        return -1;
    }
    
    // Map RX buffer
    ctx->rx_phys = DMA_MEM_BASE;
    ctx->rx_buffer = mmap(NULL, FRAME_BYTES, PROT_READ | PROT_WRITE,
                         MAP_SHARED, ctx->mem_fd, ctx->rx_phys);
    if (ctx->rx_buffer == MAP_FAILED) {
        perror("mmap RX buffer");
        munmap(ctx->dma_regs, 0x10000);
        close(ctx->mem_fd);
        return -1;
    }
    
    // Map TX buffer
    ctx->tx_phys = DMA_MEM_BASE + 0x10000;
    ctx->tx_buffer = mmap(NULL, FRAME_BYTES, PROT_READ | PROT_WRITE,
                         MAP_SHARED, ctx->mem_fd, ctx->tx_phys);
    if (ctx->tx_buffer == MAP_FAILED) {
        perror("mmap TX buffer");
        munmap(ctx->rx_buffer, FRAME_BYTES);
        munmap(ctx->dma_regs, 0x10000);
        close(ctx->mem_fd);
        return -1;
    }
    
    // Reset DMA
    DMA_WR(ctx, S2MM_CTRL, CTRL_RESET);
    DMA_WR(ctx, MM2S_CTRL, CTRL_RESET);
    usleep(100);
    
    ctx->initialized = true;
    printf("DMA initialized (frame: %d samples, %d bytes)\n",
           SAMPLES_PER_FRAME, FRAME_BYTES);
    
    return 0;
}

// Capture one audio frame
int dma_capture_frame(dma_ctx_t *ctx, int32_t *buffer) {
    if (!ctx->initialized) return -1;
    
    // Start S2MM
    DMA_WR(ctx, S2MM_CTRL, CTRL_RUN);
    DMA_WR(ctx, S2MM_DA, ctx->rx_phys);
    DMA_WR(ctx, S2MM_LENGTH, FRAME_BYTES);
    
    // Wait for completion
    int timeout = 1000;
    while (!(DMA_RD(ctx, S2MM_STATUS) & STAT_IDLE) && timeout-- > 0) {
        usleep(100);
    }
    
    if (timeout <= 0) {
        fprintf(stderr, "DMA capture timeout\n");
        return -1;
    }
    
    // Copy data
    memcpy(buffer, ctx->rx_buffer, FRAME_BYTES);
    
    return 0;
}

// Playback one audio frame
int dma_playback_frame(dma_ctx_t *ctx, const int32_t *buffer) {
    if (!ctx->initialized) return -1;
    
    // Copy data
    memcpy(ctx->tx_buffer, buffer, FRAME_BYTES);
    
    // Start MM2S
    DMA_WR(ctx, MM2S_CTRL, CTRL_RUN);
    DMA_WR(ctx, MM2S_SA, ctx->tx_phys);
    DMA_WR(ctx, MM2S_LENGTH, FRAME_BYTES);
    
    // Wait for completion
    int timeout = 1000;
    while (!(DMA_RD(ctx, MM2S_STATUS) & STAT_IDLE) && timeout-- > 0) {
        usleep(100);
    }
    
    if (timeout <= 0) {
        fprintf(stderr, "DMA playback timeout\n");
        return -1;
    }
    
    return 0;
}

// Cleanup
void dma_cleanup(dma_ctx_t *ctx) {
    if (ctx->initialized) {
        if (ctx->tx_buffer) munmap(ctx->tx_buffer, FRAME_BYTES);
        if (ctx->rx_buffer) munmap(ctx->rx_buffer, FRAME_BYTES);
        if (ctx->dma_regs) munmap(ctx->dma_regs, 0x10000);
        if (ctx->mem_fd >= 0) close(ctx->mem_fd);
        ctx->initialized = false;
    }
}
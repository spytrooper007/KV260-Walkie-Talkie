#include "audio_dma.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>

// DMA Register Offsets
#define MM2S_CTRL       0x00
#define MM2S_STATUS     0x04
#define MM2S_SA         0x18
#define MM2S_LENGTH     0x28

#define S2MM_CTRL       0x30
#define S2MM_STATUS     0x34
#define S2MM_DA         0x48
#define S2MM_LENGTH     0x58

// Control register bits
#define CTRL_RUN        0x00000001
#define CTRL_RESET      0x00000004

// Status register bits
#define STAT_HALTED     0x00000001
#define STAT_IDLE       0x00000002
#define STAT_IOC        0x00001000  // Interrupt on complete

// Register access macros
#define DMA_WRITE(ctx, offset, value) \
    (*((volatile uint32_t *)((ctx)->dma_regs + (offset))) = (value))

#define DMA_READ(ctx, offset) \
    (*((volatile uint32_t *)((ctx)->dma_regs + (offset))))

// Initialize DMA
int dma_init(dma_ctx_t *ctx) {
    memset(ctx, 0, sizeof(dma_ctx_t));
    
    // Open /dev/mem
    ctx->mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (ctx->mem_fd < 0) {
        perror("Failed to open /dev/mem");
        return -1;
    }
    
    // Map DMA registers
    ctx->dma_regs = mmap(NULL, 0x10000, PROT_READ | PROT_WRITE,
                         MAP_SHARED, ctx->mem_fd, DMA_BASE_ADDR);
    if (ctx->dma_regs == MAP_FAILED) {
        perror("Failed to map DMA registers");
        close(ctx->mem_fd);
        return -1;
    }
    
    // Map RX buffer (for capture from microphone)
    ctx->rx_phys_addr = DMA_MEM_BASE;
    ctx->rx_buffer = mmap(NULL, FRAME_BYTES, PROT_READ | PROT_WRITE,
                         MAP_SHARED, ctx->mem_fd, ctx->rx_phys_addr);
    if (ctx->rx_buffer == MAP_FAILED) {
        perror("Failed to map RX buffer");
        munmap(ctx->dma_regs, 0x10000);
        close(ctx->mem_fd);
        return -1;
    }
    
    // Map TX buffer (for playback to speaker)
    ctx->tx_phys_addr = DMA_MEM_BASE + 0x10000;  // Offset from RX buffer
    ctx->tx_buffer = mmap(NULL, FRAME_BYTES, PROT_READ | PROT_WRITE,
                         MAP_SHARED, ctx->mem_fd, ctx->tx_phys_addr);
    if (ctx->tx_buffer == MAP_FAILED) {
        perror("Failed to map TX buffer");
        munmap(ctx->rx_buffer, FRAME_BYTES);
        munmap(ctx->dma_regs, 0x10000);
        close(ctx->mem_fd);
        return -1;
    }
    
    ctx->initialized = true;
    
    printf("DMA initialised:\n");
    printf("  Registers: 0x%08X\n", DMA_BASE_ADDR);
    printf("  RX Buffer: 0x%08X\n", ctx->rx_phys_addr);
    printf("  TX Buffer: 0x%08X\n", ctx->tx_phys_addr);
    printf("  Frame size: %d bytes (%d samples)\n", 
           FRAME_BYTES, SAMPLES_PER_FRAME);
    
    return 0;
}

// Reset DMA channels
int dma_reset(dma_ctx_t *ctx) {
    if (!ctx->initialized) {
        fprintf(stderr, "DMA not initialized\n");
        return -1;
    }
    
    // Reset both channels
    DMA_WRITE(ctx, S2MM_CTRL, CTRL_RESET);
    DMA_WRITE(ctx, MM2S_CTRL, CTRL_RESET);
    usleep(100);
    
    // Wait for reset to complete
    int timeout = 1000;
    while (timeout-- > 0) {
        uint32_t s2mm_status = DMA_READ(ctx, S2MM_STATUS);
        uint32_t mm2s_status = DMA_READ(ctx, MM2S_STATUS);
        
        if ((s2mm_status & STAT_HALTED) && (mm2s_status & STAT_HALTED)) {
            break;
        }
        usleep(1);
    }
    
    if (timeout <= 0) {
        fprintf(stderr, "DMA reset timeout\n");
        return -1;
    }
    
    printf("DMA reset complete\n");
    return 0;
}

// Start audio capture (microphone -> memory)
int dma_start_capture(dma_ctx_t *ctx, int32_t *buffer, size_t bytes) {
    if (!ctx->initialized) {
        fprintf(stderr, "DMA not initialised\n");
        return -1;
    }
    
    // Calculate physical address
    uintptr_t offset = (uintptr_t)buffer - (uintptr_t)ctx->rx_buffer;
    uint32_t phys_addr = ctx->rx_phys_addr + offset;
    
    // Start S2MM channel
    DMA_WRITE(ctx, S2MM_CTRL, CTRL_RUN);
    DMA_WRITE(ctx, S2MM_DA, phys_addr);
    DMA_WRITE(ctx, S2MM_LENGTH, bytes);  // This starts the transfer
    
    return 0;
}

// Start audio playback (memory -> speaker)
int dma_start_playback(dma_ctx_t *ctx, const int32_t *buffer, size_t bytes) {
    if (!ctx->initialized) {
        fprintf(stderr, "DMA not initialised\n");
        return -1;
    }
    
    // Copy data to TX buffer
    memcpy(ctx->tx_buffer, buffer, bytes);
    
    // Start MM2S channel
    DMA_WRITE(ctx, MM2S_CTRL, CTRL_RUN);
    DMA_WRITE(ctx, MM2S_SA, ctx->tx_phys_addr);
    DMA_WRITE(ctx, MM2S_LENGTH, bytes);  // This starts the transfer
    
    return 0;
}

// Check if capture is busy
bool dma_capture_busy(dma_ctx_t *ctx) {
    if (!ctx->initialized) return false;
    
    uint32_t status = DMA_READ(ctx, S2MM_STATUS);
    return !(status & STAT_IDLE);
}

// Check if playback is busy
bool dma_playback_busy(dma_ctx_t *ctx) {
    if (!ctx->initialized) return false;
    
    uint32_t status = DMA_READ(ctx, MM2S_STATUS);
    return !(status & STAT_IDLE);
}

// Wait for capture to complete
int dma_wait_capture(dma_ctx_t *ctx, int timeout_ms) {
    int elapsed = 0;
    
    while (dma_capture_busy(ctx) && elapsed < timeout_ms) {
        usleep(100);
        elapsed++;
    }
    
    if (elapsed >= timeout_ms) {
        fprintf(stderr, "Capture timeout\n");
        return -1;
    }
    
    return 0;
}

// Wait for playback to complete
int dma_wait_playback(dma_ctx_t *ctx, int timeout_ms) {
    int elapsed = 0;
    
    while (dma_playback_busy(ctx) && elapsed < timeout_ms) {
        usleep(100);
        elapsed++;
    }
    
    if (elapsed >= timeout_ms) {
        fprintf(stderr, "Playback timeout\n");
        return -1;
    }
    
    return 0;
}

// Get RX buffer pointer
int32_t* dma_get_rx_buffer(dma_ctx_t *ctx) {
    if (!ctx->initialized) return NULL;
    return (int32_t *)ctx->rx_buffer;
}

// Get TX buffer pointer
int32_t* dma_get_tx_buffer(dma_ctx_t *ctx) {
    if (!ctx->initialized) return NULL;
    return (int32_t *)ctx->tx_buffer;
}

// Cleanup DMA
void dma_cleanup(dma_ctx_t *ctx) {
    if (ctx->initialized) {
        if (ctx->tx_buffer && ctx->tx_buffer != MAP_FAILED) {
            munmap(ctx->tx_buffer, FRAME_BYTES);
        }
        if (ctx->rx_buffer && ctx->rx_buffer != MAP_FAILED) {
            munmap(ctx->rx_buffer, FRAME_BYTES);
        }
        if (ctx->dma_regs && ctx->dma_regs != MAP_FAILED) {
            munmap(ctx->dma_regs, 0x10000);
        }
        if (ctx->mem_fd >= 0) {
            close(ctx->mem_fd);
        }
        ctx->initialized = false;
    }
}
#ifndef AUDIO_DMA_H
#define AUDIO_DMA_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// DMA configuration
#define DMA_BASE_ADDR       0xA0000000
#define DMA_MEM_BASE        0x70000000
#define DMA_MEM_SIZE        0x02000000      // 32MB

// Audio buffer configuration
#define SAMPLES_PER_FRAME   960             // 20ms at 48kHz
#define BYTES_PER_SAMPLE    4               // 32-bit samples
#define FRAME_BYTES         (SAMPLES_PER_FRAME * BYTES_PER_SAMPLE)

// DMA context
typedef struct {
    int mem_fd;
    void *dma_regs;
    void *rx_buffer;        // S2MM buffer (microphone)
    void *tx_buffer;        // MM2S buffer (speaker)
    uint32_t rx_phys_addr;
    uint32_t tx_phys_addr;
    bool initialized;
} dma_ctx_t;

// Initialize DMA (map registers and buffers)
int dma_init(dma_ctx_t *ctx);

// Start audio capture (S2MM - Stream to Memory)
int dma_start_capture(dma_ctx_t *ctx, int32_t *buffer, size_t bytes);

// Start audio playback (MM2S - Memory to Stream)
int dma_start_playback(dma_ctx_t *ctx, const int32_t *buffer, size_t bytes);

// Wait for capture to complete
int dma_wait_capture(dma_ctx_t *ctx, int timeout_ms);

// Wait for playback to complete
int dma_wait_playback(dma_ctx_t *ctx, int timeout_ms);

// Check if capture is busy
bool dma_capture_busy(dma_ctx_t *ctx);

// Check if playback is busy
bool dma_playback_busy(dma_ctx_t *ctx);

// Reset DMA channels
int dma_reset(dma_ctx_t *ctx);

// Cleanup DMA
void dma_cleanup(dma_ctx_t *ctx);

// Get buffer pointers
int32_t* dma_get_rx_buffer(dma_ctx_t *ctx);
int32_t* dma_get_tx_buffer(dma_ctx_t *ctx);

#endif // AUDIO_DMA_H
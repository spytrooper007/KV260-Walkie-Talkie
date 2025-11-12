#ifndef AUDIO_DMA_H
#define AUDIO_DMA_H

#include <stdint.h>
#include <stdbool.h>

// DMA configuration
#define DMA_BASE_ADDR       0xA0000000
#define DMA_MEM_BASE        0x70000000

// Audio parameters (must match Opus)
#define SAMPLES_PER_FRAME   960
#define BYTES_PER_SAMPLE    4
#define FRAME_BYTES         (SAMPLES_PER_FRAME * BYTES_PER_SAMPLE)

// DMA context
typedef struct {
    int mem_fd;
    void *dma_regs;
    int32_t *rx_buffer;
    int32_t *tx_buffer;
    uint32_t rx_phys;
    uint32_t tx_phys;
    bool initialized;
} dma_ctx_t;

// Initialize DMA
int dma_init(dma_ctx_t *ctx);

// Capture audio frame from microphone (blocking)
int dma_capture_frame(dma_ctx_t *ctx, int32_t *buffer);

// Playback audio frame to speaker (blocking)
int dma_playback_frame(dma_ctx_t *ctx, const int32_t *buffer);

// Cleanup
void dma_cleanup(dma_ctx_t *ctx);

#endif // AUDIO_DMA_H
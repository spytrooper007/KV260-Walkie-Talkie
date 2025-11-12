#ifndef OPUS_HELPER_H
#define OPUS_HELPER_H

#include <stdint.h>
#include <stdbool.h>
#include <opus/opus.h>

// Audio parameters
#define SAMPLE_RATE         48000
#define CHANNELS            1
#define FRAME_SIZE          960      // 20ms at 48kHz
#define MAX_PACKET_SIZE     4000
#define BITRATE             24000    // 24 kbps

// Opus context structures
typedef struct {
    OpusEncoder *encoder;
    OpusDecoder *decoder;
    bool initialized;
} opus_ctx_t;

// Initialize encoder and decoder
int opus_init(opus_ctx_t *ctx);

// Encode PCM samples to Opus
int opus_encode_frame(opus_ctx_t *ctx, 
                      const int16_t *pcm_in,
                      uint8_t *opus_out);

// Decode Opus packet to PCM
int opus_decode_frame(opus_ctx_t *ctx,
                      const uint8_t *opus_in,
                      int packet_size,
                      int16_t *pcm_out);

// Cleanup
void opus_cleanup(opus_ctx_t *ctx);

// Utility: Convert int32 DMA samples to int16 for Opus
void convert_i32_to_i16(const int32_t *in, int16_t *out, int samples);

// Utility: Convert int16 Opus output to int32 for DMA
void convert_i16_to_i32(const int16_t *in, int32_t *out, int samples);

#endif // OPUS_HELPER_H
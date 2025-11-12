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
#define BITRATE             24000    // 24 kbps for speech

// Opus context structures
typedef struct {
    OpusEncoder *encoder;
    int bitrate;
    bool initialized;
} opus_enc_ctx_t;

typedef struct {
    OpusDecoder *decoder;
    bool initialized;
} opus_dec_ctx_t;

// Initialize encoder
int opus_enc_init(opus_enc_ctx_t *ctx, int bitrate);

// Encode PCM samples to Opus
int opus_encode_frame(opus_enc_ctx_t *ctx, 
                      const int16_t *pcm_in,
                      int frame_size,
                      uint8_t *opus_out,
                      int max_bytes);

// Cleanup encoder
void opus_enc_cleanup(opus_enc_ctx_t *ctx);

// Initialize decoder
int opus_dec_init(opus_dec_ctx_t *ctx);

// Decode Opus packet to PCM
int opus_decode_frame(opus_dec_ctx_t *ctx,
                      const uint8_t *opus_in,
                      int packet_size,
                      int16_t *pcm_out,
                      int frame_size);

// Handle packet loss (FEC)
int opus_decode_lost(opus_dec_ctx_t *ctx,
                     int16_t *pcm_out,
                     int frame_size);

// Cleanup decoder
void opus_dec_cleanup(opus_dec_ctx_t *ctx);

// Utility: Convert int32 DMA samples to int16 for Opus
void convert_i32_to_i16(const int32_t *in, int16_t *out, int samples);

// Utility: Convert int16 Opus output to int32 for DMA
void convert_i16_to_i32(const int16_t *in, int32_t *out, int samples);

#endif // OPUS_HELPER_H
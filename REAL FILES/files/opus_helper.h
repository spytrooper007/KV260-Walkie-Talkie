#ifndef OPUS_HELPER_H
#define OPUS_HELPER_H

#include <stdint.h>
#include <stdbool.h>
#include <opus/opus.h>

// Audio parameters
#define SAMPLE_RATE         44000
#define CHANNELS            1
#define FRAME_SIZE          880      // 44000 * 0.02 (20ms frames, good balance of latency/quality)
#define MAX_PACKET_SIZE     1024     // Max bytes for Opus packet
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

int opus_enc_init(opus_enc_ctx_t *ctx, int bitrate);
int opus_encode_frame(opus_enc_ctx_t *ctx, 
                      const int16_t *pcm_in,
                      int frame_size,
                      uint8_t *opus_out,
                      int max_bytes);
void opus_enc_cleanup(opus_enc_ctx_t *ctx);
int opus_dec_init(opus_dec_ctx_t *ctx);
int opus_decode_frame(opus_dec_ctx_t *ctx,
                      const uint8_t *opus_in,
                      int packet_size,
                      int16_t *pcm_out,
                      int frame_size);
int opus_decode_lost(opus_dec_ctx_t *ctx,
                     int16_t *pcm_out,
                     int frame_size);
void opus_dec_cleanup(opus_dec_ctx_t *ctx);
void convert_i32_to_i16(const int32_t *in, int16_t *out, int samples);
void convert_i16_to_i32(const int16_t *in, int32_t *out, int samples);

#endif // OPUS_HELPER_H
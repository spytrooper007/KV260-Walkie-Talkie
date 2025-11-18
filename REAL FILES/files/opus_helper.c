#include "opus_helper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Initialize Opus encoder
int opus_enc_init(opus_enc_ctx_t *ctx, int bitrate) {
    int error;
    
    ctx->encoder = opus_encoder_create(SAMPLE_RATE, CHANNELS, 
                                       OPUS_APPLICATION_VOIP, &error);
    if (error != OPUS_OK) {
        fprintf(stderr, "Opus encoder create failed: %s\n", 
                opus_strerror(error));
        return -1;
    }
    
    // Set bitrate
    opus_encoder_ctl(ctx->encoder, OPUS_SET_BITRATE(bitrate));
    ctx->bitrate = bitrate;
    
    // Optimize for low latency
    opus_encoder_ctl(ctx->encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    opus_encoder_ctl(ctx->encoder, OPUS_SET_COMPLEXITY(5));
    opus_encoder_ctl(ctx->encoder, OPUS_SET_DTX(0));  // Disable discontinuous transmission
    
    // Enable FEC for packet loss resilience
    opus_encoder_ctl(ctx->encoder, OPUS_SET_INBAND_FEC(1));
    opus_encoder_ctl(ctx->encoder, OPUS_SET_PACKET_LOSS_PERC(5));
    
    ctx->initialized = true;
    printf("Opus encoder initialised: %d Hz, %d ch, %d bps\n",
           SAMPLE_RATE, CHANNELS, bitrate);
    
    return 0;
}

// Encode PCM samples to Opus
int opus_encode_frame(opus_enc_ctx_t *ctx, 
                      const int16_t *pcm_in,
                      int frame_size,
                      uint8_t *opus_out,
                      int max_bytes) {
    if (!ctx->initialized) {
        fprintf(stderr, "Encoder not initialised\n");
        return -1;
    }
    
    int encoded_bytes = opus_encode(ctx->encoder, pcm_in, frame_size,
                                    opus_out, max_bytes);
    
    if (encoded_bytes < 0) {
        fprintf(stderr, "Opus encode error: %s\n", 
                opus_strerror(encoded_bytes));
        return -1;
    }
    
    return encoded_bytes;
}

// Cleanup encoder
void opus_enc_cleanup(opus_enc_ctx_t *ctx) {
    if (ctx->initialized && ctx->encoder) {
        opus_encoder_destroy(ctx->encoder);
        ctx->encoder = NULL;
        ctx->initialized = false;
    }
}

// Initialize Opus decoder
int opus_dec_init(opus_dec_ctx_t *ctx) {
    int error;
    
    ctx->decoder = opus_decoder_create(SAMPLE_RATE, CHANNELS, &error);
    if (error != OPUS_OK) {
        fprintf(stderr, "Opus decoder create failed: %s\n",
                opus_strerror(error));
        return -1;
    }
    
    ctx->initialized = true;
    printf("Opus decoder initialised: %d Hz, %d ch\n",
           SAMPLE_RATE, CHANNELS);
    
    return 0;
}

// Decode Opus packet to PCM
int opus_decode_frame(opus_dec_ctx_t *ctx,
                      const uint8_t *opus_in,
                      int packet_size,
                      int16_t *pcm_out,
                      int frame_size) {
    if (!ctx->initialized) {
        fprintf(stderr, "Decoder not initialised\n");
        return -1;
    }
    
    int decoded_samples = opus_decode(ctx->decoder, opus_in, packet_size,
                                      pcm_out, frame_size, 0);
    
    if (decoded_samples < 0) {
        fprintf(stderr, "Opus decode error: %s\n",
                opus_strerror(decoded_samples));
        return -1;
    }
    
    return decoded_samples;
}

// Handle packet loss with FEC
int opus_decode_lost(opus_dec_ctx_t *ctx,
                     int16_t *pcm_out,
                     int frame_size) {
    if (!ctx->initialized) {
        fprintf(stderr, "Decoder not initialised\n");
        return -1;
    }
    
    // Decode with FEC (NULL packet indicates loss)
    int decoded_samples = opus_decode(ctx->decoder, NULL, 0,
                                      pcm_out, frame_size, 1);
    
    if (decoded_samples < 0) {
        fprintf(stderr, "Opus FEC decode error: %s\n",
                opus_strerror(decoded_samples));
        return -1;
    }
    
    return decoded_samples;
}

// Cleanup decoder
void opus_dec_cleanup(opus_dec_ctx_t *ctx) {
    if (ctx->initialized && ctx->decoder) {
        opus_decoder_destroy(ctx->decoder);
        ctx->decoder = NULL;
        ctx->initialized = false;
    }
}

// Convert 32-bit DMA samples to 16-bit for Opus
void convert_i32_to_i16(const int32_t *in, int16_t *out, int samples) {
    for (int i = 0; i < samples; i++) {
        // Shift down from 32-bit to 16-bit (keep upper 16 bits)
        out[i] = (int16_t)(in[i] >> 16);
    }
}

// Convert 16-bit Opus output to 32-bit for DMA
void convert_i16_to_i32(const int16_t *in, int32_t *out, int samples) {
    for (int i = 0; i < samples; i++) {
        // Shift up from 16-bit to 32-bit
        out[i] = ((int32_t)in[i]) << 16;
    }
}
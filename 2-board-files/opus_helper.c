#include "opus_helper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Initialize Opus encoder and decoder
int opus_init(opus_ctx_t *ctx) {
    int error;
    
    memset(ctx, 0, sizeof(opus_ctx_t));
    
    // Create encoder
    ctx->encoder = opus_encoder_create(SAMPLE_RATE, CHANNELS, 
                                       OPUS_APPLICATION_VOIP, &error);
    if (error != OPUS_OK) {
        fprintf(stderr, "Opus encoder create failed: %s\n", 
                opus_strerror(error));
        return -1;
    }
    
    // Create decoder
    ctx->decoder = opus_decoder_create(SAMPLE_RATE, CHANNELS, &error);
    if (error != OPUS_OK) {
        fprintf(stderr, "Opus decoder create failed: %s\n",
                opus_strerror(error));
        opus_encoder_destroy(ctx->encoder);
        return -1;
    }
    
    // Configure encoder for low latency
    opus_encoder_ctl(ctx->encoder, OPUS_SET_BITRATE(BITRATE));
    opus_encoder_ctl(ctx->encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    opus_encoder_ctl(ctx->encoder, OPUS_SET_COMPLEXITY(5));
    opus_encoder_ctl(ctx->encoder, OPUS_SET_INBAND_FEC(1));
    opus_encoder_ctl(ctx->encoder, OPUS_SET_PACKET_LOSS_PERC(5));
    
    ctx->initialized = true;
    
    printf("Opus initialized: %d Hz, %d ch, %d bps\n",
           SAMPLE_RATE, CHANNELS, BITRATE);
    
    return 0;
}

// Encode PCM to Opus
int opus_encode_frame(opus_ctx_t *ctx, 
                      const int16_t *pcm_in,
                      uint8_t *opus_out) {
    if (!ctx->initialized) {
        return -1;
    }
    
    int bytes = opus_encode(ctx->encoder, pcm_in, FRAME_SIZE,
                           opus_out, MAX_PACKET_SIZE);
    
    if (bytes < 0) {
        fprintf(stderr, "Opus encode error: %s\n", opus_strerror(bytes));
        return -1;
    }
    
    return bytes;
}

// Decode Opus to PCM
int opus_decode_frame(opus_ctx_t *ctx,
                      const uint8_t *opus_in,
                      int packet_size,
                      int16_t *pcm_out) {
    if (!ctx->initialized) {
        return -1;
    }
    
    int samples = opus_decode(ctx->decoder, opus_in, packet_size,
                             pcm_out, FRAME_SIZE, 0);
    
    if (samples < 0) {
        fprintf(stderr, "Opus decode error: %s\n", opus_strerror(samples));
        return -1;
    }
    
    return samples;
}

// Cleanup
void opus_cleanup(opus_ctx_t *ctx) {
    if (ctx->initialized) {
        if (ctx->encoder) opus_encoder_destroy(ctx->encoder);
        if (ctx->decoder) opus_decoder_destroy(ctx->decoder);
        ctx->initialized = false;
    }
}

// Convert 32-bit to 16-bit
void convert_i32_to_i16(const int32_t *in, int16_t *out, int samples) {
    for (int i = 0; i < samples; i++) {
        out[i] = (int16_t)(in[i] >> 16);
    }
}

// Convert 16-bit to 32-bit
void convert_i16_to_i32(const int16_t *in, int32_t *out, int samples) {
    for (int i = 0; i < samples; i++) {
        out[i] = ((int32_t)in[i]) << 16;
    }
}
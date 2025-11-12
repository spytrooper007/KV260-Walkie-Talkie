/*
 * walkietalkie.c - FPGA Multi-Board Walkie-Talkie System
 * 
 * Main application integrating:
 * - Opus codec for audio compression
 * - DMA for low-latency audio I/O
 * - UDP multicast for networking
 * - GPIO for PTT control
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>

#include "opus_helper.h"
#include "network.h"
#include "audio_dma.h"
#include "gpio_ptt.h"

// Application state
typedef struct {
    // Component contexts
    dma_ctx_t dma;
    network_ctx_t net;
    gpio_ctx_t gpio;
    opus_enc_ctx_t encoder;
    opus_dec_ctx_t decoder;
    
    // State
    bool running;
    bool transmitting;
    uint32_t board_id;
    
    // Threads
    pthread_t tx_thread;
    pthread_t rx_thread;
    
    // Statistics
    uint64_t frames_sent;
    uint64_t frames_received;
    uint64_t frames_dropped;
} app_state_t;

static app_state_t app = {0};

// Signal handler for clean shutdown
void signal_handler(int sig) {
    printf("\n[Signal %d] Shutting down...\n", sig);
    app.running = false;
}

// Transmitter thread
void *tx_thread_func(void *arg) {
    printf("TX thread started\n");
    
    bool last_ptt = false;
    int32_t *dma_buffer = dma_get_rx_buffer(&app.dma);
    int16_t pcm_i16[FRAME_SIZE];
    uint8_t opus_packet[MAX_PACKET_SIZE];
    
    while (app.running) {
        bool ptt = gpio_read_ptt(&app.gpio);
        
        // PTT pressed - start transmission
        if (ptt && !last_ptt) {
            app.transmitting = true;
            gpio_set_tx_led(&app.gpio, true);
            printf("\n[TX START]\n");
            
            // Send START packet
            network_send(&app.net, NULL, 0, PKT_FLAG_START);
        }
        
        // PTT released - end transmission
        if (!ptt && last_ptt) {
            printf("[TX END]\n\n");
            
            // Send END packet
            network_send(&app.net, NULL, 0, PKT_FLAG_END);
            
            app.transmitting = false;
            gpio_set_tx_led(&app.gpio, false);
        }
        
        // Transmit audio while PTT held
        if (app.transmitting && ptt) {
            // Capture audio from microphone
            if (dma_start_capture(&app.dma, dma_buffer, FRAME_BYTES) < 0) {
                usleep(10000);
                continue;
            }
            
            // Wait for DMA completion
            if (dma_wait_capture(&app.dma, 100) < 0) {
                fprintf(stderr, "DMA capture timeout\n");
                usleep(10000);
                continue;
            }
            
            // Convert 32-bit DMA samples to 16-bit for Opus
            convert_i32_to_i16(dma_buffer, pcm_i16, FRAME_SIZE);
            
            // Encode with Opus
            int opus_size = opus_encode_frame(&app.encoder, pcm_i16, 
                                             FRAME_SIZE, opus_packet, 
                                             MAX_PACKET_SIZE);
            
            if (opus_size > 0) {
                // Send over network
                if (network_send(&app.net, opus_packet, opus_size, 0) > 0) {
                    app.frames_sent++;
                    
                    if (app.frames_sent % 50 == 0) {
                        printf(".");
                        fflush(stdout);
                    }
                }
            }
            
            // Maintain ~20ms frame timing
            usleep(18000);  // Slightly less than 20ms to account for processing
        } else {
            // Not transmitting - poll PTT at lower rate
            usleep(10000);
        }
        
        last_ptt = ptt;
    }
    
    printf("TX thread stopped\n");
    return NULL;
}

// Receiver thread
void *rx_thread_func(void *arg) {
    printf("RX thread started\n");
    
    network_packet_t packet;
    int16_t pcm_i16[FRAME_SIZE];
    int32_t *dma_buffer = dma_get_tx_buffer(&app.dma);
    bool receiving = false;
    uint32_t current_sender = 0;
    
    while (app.running) {
        // Receive packet (50ms timeout)
        int recv_size = network_recv(&app.net, &packet, 50);
        
        if (recv_size <= 0) {
            // No packet received
            usleep(1000);
            continue;
        }
        
        // Self-mute: ignore our own packets
        if (packet.board_id == app.board_id) {
            continue;
        }
        
        // Don't play while transmitting
        if (app.transmitting) {
            continue;
        }
        
        // Handle START packet
        if (packet.flags & PKT_FLAG_START) {
            receiving = true;
            current_sender = packet.board_id;
            gpio_set_rx_led(&app.gpio, true);
            printf("\n[RX START - Board %u]\n", packet.board_id);
            continue;
        }
        
        // Handle END packet
        if (packet.flags & PKT_FLAG_END) {
            receiving = false;
            gpio_set_rx_led(&app.gpio, false);
            printf("[RX END - Board %u]\n\n", current_sender);
            continue;
        }
        
        // Process audio packet
        if (receiving && packet.opus_size > 0) {
            // Decode Opus to PCM
            int decoded_samples = opus_decode_frame(&app.decoder,
                                                    packet.opus_data,
                                                    packet.opus_size,
                                                    pcm_i16,
                                                    FRAME_SIZE);
            
            if (decoded_samples == FRAME_SIZE) {
                // Convert 16-bit PCM to 32-bit for DMA
                convert_i16_to_i32(pcm_i16, dma_buffer, FRAME_SIZE);
                
                // Play audio through speaker
                if (dma_start_playback(&app.dma, dma_buffer, FRAME_BYTES) >= 0) {
                    // Wait for playback (with timeout)
                    dma_wait_playback(&app.dma, 100);
                    
                    app.frames_received++;
                    
                    if (app.frames_received % 50 == 0) {
                        printf(":");
                        fflush(stdout);
                    }
                }
            } else {
                app.frames_dropped++;
            }
        }
    }
    
    printf("RX thread stopped\n");
    return NULL;
}

// Initialize all subsystems
int init_system(void) {
    printf("Initializing walkie-talkie system...\n\n");
    
    // Get board ID
    app.board_id = network_get_board_id();
    printf("Board ID: %u\n\n", app.board_id);
    
    // Initialize GPIO
    printf("Initializing GPIO...\n");
    if (gpio_init(&app.gpio) < 0) {
        fprintf(stderr, "GPIO initialization failed\n");
        return -1;
    }
    printf("✓ GPIO ready\n\n");
    
    // Initialize DMA
    printf("Initializing DMA...\n");
    if (dma_init(&app.dma) < 0) {
        fprintf(stderr, "DMA initialization failed\n");
        gpio_cleanup(&app.gpio);
        return -1;
    }
    if (dma_reset(&app.dma) < 0) {
        fprintf(stderr, "DMA reset failed\n");
        dma_cleanup(&app.dma);
        gpio_cleanup(&app.gpio);
        return -1;
    }
    printf("✓ DMA ready\n\n");
    
    // Initialize Opus encoder
    printf("Initializing Opus encoder...\n");
    if (opus_enc_init(&app.encoder, BITRATE) < 0) {
        fprintf(stderr, "Opus encoder initialization failed\n");
        dma_cleanup(&app.dma);
        gpio_cleanup(&app.gpio);
        return -1;
    }
    printf("✓ Encoder ready\n\n");
    
    // Initialize Opus decoder
    printf("Initializing Opus decoder...\n");
    if (opus_dec_init(&app.decoder) < 0) {
        fprintf(stderr, "Opus decoder initialization failed\n");
        opus_enc_cleanup(&app.encoder);
        dma_cleanup(&app.dma);
        gpio_cleanup(&app.gpio);
        return -1;
    }
    printf("✓ Decoder ready\n\n");
    
    // Initialize network
    printf("Initializing network...\n");
    if (network_init(&app.net, app.board_id) < 0) {
        fprintf(stderr, "Network initialization failed\n");
        opus_dec_cleanup(&app.decoder);
        opus_enc_cleanup(&app.encoder);
        dma_cleanup(&app.dma);
        gpio_cleanup(&app.gpio);
        return -1;
    }
    printf("✓ Network ready\n\n");
    
    return 0;
}

// Cleanup all subsystems
void cleanup_system(void) {
    printf("\nCleaning up...\n");
    
    gpio_leds_off(&app.gpio);
    network_cleanup(&app.net);
    opus_dec_cleanup(&app.decoder);
    opus_enc_cleanup(&app.encoder);
    dma_cleanup(&app.dma);
    gpio_cleanup(&app.gpio);
    
    printf("Cleanup complete\n");
}

// Print statistics
void print_stats(void) {
    printf("\n╔═══════════════════════════════════════╗\n");
    printf("║         System Statistics            ║\n");
    printf("╚═══════════════════════════════════════╝\n");
    printf("  Frames sent:     %lu\n", app.frames_sent);
    printf("  Frames received: %lu\n", app.frames_received);
    printf("  Frames dropped:  %lu\n", app.frames_dropped);
    
    if (app.frames_received > 0) {
        double drop_rate = (double)app.frames_dropped / 
                          (app.frames_received + app.frames_dropped) * 100.0;
        printf("  Drop rate:       %.2f%%\n", drop_rate);
    }
    printf("\n");
}

// Main
int main(int argc, char *argv[]) {
    printf("╔═══════════════════════════════════════════╗\n");
    printf("║  FPGA Walkie-Talkie System v2.0 (Opus)  ║\n");
    printf("╚═══════════════════════════════════════════╝\n\n");
    
    // Override board ID if provided
    if (argc > 1) {
        app.board_id = atoi(argv[1]);
    }
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize system
    if (init_system() < 0) {
        fprintf(stderr, "System initialization failed\n");
        return 1;
    }
    
    // Start threads
    app.running = true;
    app.transmitting = false;
    
    if (pthread_create(&app.tx_thread, NULL, tx_thread_func, NULL) != 0) {
        perror("Failed to create TX thread");
        cleanup_system();
        return 1;
    }
    
    if (pthread_create(&app.rx_thread, NULL, rx_thread_func, NULL) != 0) {
        perror("Failed to create RX thread");
        app.running = false;
        pthread_join(app.tx_thread, NULL);
        cleanup_system();
        return 1;
    }
    
    printf("╔═══════════════════════════════════════════╗\n");
    printf("║           SYSTEM READY                   ║\n");
    printf("╠═══════════════════════════════════════════╣\n");
    printf("║  • Press PTT to transmit                 ║\n");
    printf("║  • Press Ctrl+C to exit                  ║\n");
    printf("║                                          ║\n");
    printf("║  Legend: . = TX frame  : = RX frame     ║\n");
    printf("╚═══════════════════════════════════════════╝\n\n");
    
    // Status monitoring loop
    while (app.running) {
        sleep(30);
        
        // Print periodic stats
        printf("\n[Stats] TX: %lu  RX: %lu  Drop: %lu\n",
               app.frames_sent, app.frames_received, app.frames_dropped);
    }
    
    // Wait for threads to finish
    printf("\nWaiting for threads to finish...\n");
    pthread_join(app.tx_thread, NULL);
    pthread_join(app.rx_thread, NULL);
    
    // Print final statistics
    print_stats();
    
    // Cleanup
    cleanup_system();
    
    printf("Goodbye!\n");
    return 0;
}
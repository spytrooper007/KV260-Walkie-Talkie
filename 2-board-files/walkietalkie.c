/*
 * walkietalkie.c - Simple 2-Board Walkie-Talkie
 * 
 * Point-to-point audio communication between two FPGA boards
 * using Opus compression over UDP
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
    opus_ctx_t opus;
    network_ctx_t net;
    dma_ctx_t dma;
    gpio_ctx_t gpio;
    
    bool running;
    bool transmitting;
    
    pthread_t tx_thread;
    pthread_t rx_thread;
    
    uint64_t frames_sent;
    uint64_t frames_received;
} app_state_t;

static app_state_t app = {0};

// Signal handler
void signal_handler(int sig) {
    printf("\nShutdown requested\n");
    app.running = false;
}

// Transmitter thread
void *tx_thread_func(void *arg) {
    printf("TX thread started\n");
    
    int32_t dma_buf[SAMPLES_PER_FRAME];
    int16_t pcm_buf[FRAME_SIZE];
    uint8_t opus_buf[MAX_PACKET_SIZE];
    bool last_ptt = false;
    
    while (app.running) {
        bool ptt = gpio_read_ptt(&app.gpio);
        
        // PTT state changes
        if (ptt && !last_ptt) {
            app.transmitting = true;
            gpio_set_tx_led(&app.gpio, true);
            printf("\n[TX ON]\n");
        }
        
        if (!ptt && last_ptt) {
            app.transmitting = false;
            gpio_set_tx_led(&app.gpio, false);
            printf("[TX OFF]\n\n");
        }
        
        // Transmit while PTT held
        if (app.transmitting) {
            // Capture audio
            if (dma_capture_frame(&app.dma, dma_buf) < 0) {
                usleep(10000);
                continue;
            }
            
            // Convert to 16-bit
            convert_i32_to_i16(dma_buf, pcm_buf, FRAME_SIZE);
            
            // Encode with Opus
            int opus_size = opus_encode_frame(&app.opus, pcm_buf, opus_buf);
            
            if (opus_size > 0) {
                // Send to peer
                network_send(&app.net, opus_buf, opus_size);
                app.frames_sent++;
                
                if (app.frames_sent % 50 == 0) {
                    printf(".");
                    fflush(stdout);
                }
            }
        } else {
            usleep(10000);  // Poll PTT at 100Hz when idle
        }
        
        last_ptt = ptt;
    }
    
    printf("TX thread stopped\n");
    return NULL;
}

// Receiver thread
void *rx_thread_func(void *arg) {
    printf("RX thread started\n");
    
    uint8_t opus_buf[MAX_PACKET_SIZE];
    int16_t pcm_buf[FRAME_SIZE];
    int32_t dma_buf[SAMPLES_PER_FRAME];
    bool receiving = false;
    
    while (app.running) {
        // Receive packet (50ms timeout)
        int size = network_recv(&app.net, opus_buf, 50);
        
        if (size <= 0) {
            if (receiving) {
                // Stop receiving if no packets for a while
                receiving = false;
                gpio_set_rx_led(&app.gpio, false);
                printf("\n[RX IDLE]\n");
            }
            continue;
        }
        
        // Don't play while transmitting
        if (app.transmitting) {
            continue;
        }
        
        // Start receiving
        if (!receiving) {
            receiving = true;
            gpio_set_rx_led(&app.gpio, true);
            printf("\n[RX ON]\n");
        }
        
        // Decode Opus
        int samples = opus_decode_frame(&app.opus, opus_buf, size, pcm_buf);
        
        if (samples == FRAME_SIZE) {
            // Convert to 32-bit
            convert_i16_to_i32(pcm_buf, dma_buf, FRAME_SIZE);
            
            // Playback
            if (dma_playback_frame(&app.dma, dma_buf) >= 0) {
                app.frames_received++;
                
                if (app.frames_received % 50 == 0) {
                    printf(":");
                    fflush(stdout);
                }
            }
        }
    }
    
    printf("RX thread stopped\n");
    return NULL;
}

// Main
int main(int argc, char *argv[]) {
    printf("╔═══════════════════════════════════════╗\n");
    printf("║  2-Board Walkie-Talkie (Opus/UDP)   ║\n");
    printf("╚═══════════════════════════════════════╝\n\n");
    
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <peer_ip>\n", argv[0]);
        fprintf(stderr, "Example: %s 192.168.1.11\n", argv[0]);
        return 1;
    }
    
    const char *peer_ip = argv[1];
    
    // Setup signals
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize GPIO
    printf("Initializing GPIO...\n");
    if (gpio_init(&app.gpio) < 0) {
        fprintf(stderr, "GPIO init failed\n");
        return 1;
    }
    
    // Initialize DMA
    printf("Initializing DMA...\n");
    if (dma_init(&app.dma) < 0) {
        fprintf(stderr, "DMA init failed\n");
        gpio_cleanup(&app.gpio);
        return 1;
    }
    
    // Initialize Opus
    printf("Initializing Opus...\n");
    if (opus_init(&app.opus) < 0) {
        fprintf(stderr, "Opus init failed\n");
        dma_cleanup(&app.dma);
        gpio_cleanup(&app.gpio);
        return 1;
    }
    
    // Initialize Network
    printf("Initializing network...\n");
    if (network_init(&app.net, peer_ip) < 0) {
        fprintf(stderr, "Network init failed\n");
        opus_cleanup(&app.opus);
        dma_cleanup(&app.dma);
        gpio_cleanup(&app.gpio);
        return 1;
    }
    
    // Start threads
    app.running = true;
    app.transmitting = false;
    
    if (pthread_create(&app.tx_thread, NULL, tx_thread_func, NULL) != 0) {
        perror("pthread_create tx");
        goto cleanup;
    }
    
    if (pthread_create(&app.rx_thread, NULL, rx_thread_func, NULL) != 0) {
        perror("pthread_create rx");
        app.running = false;
        pthread_join(app.tx_thread, NULL);
        goto cleanup;
    }
    
    printf("\n╔═══════════════════════════════════════╗\n");
    printf("║            SYSTEM READY              ║\n");
    printf("╠═══════════════════════════════════════╣\n");
    printf("║  Peer: %-30s  ║\n", peer_ip);
    printf("║                                      ║\n");
    printf("║  • Press PTT to talk                 ║\n");
    printf("║  • Press Ctrl+C to exit              ║\n");
    printf("║                                      ║\n");
    printf("║  Legend: . = TX  : = RX              ║\n");
    printf("╚═══════════════════════════════════════╝\n\n");
    
    // Status loop
    while (app.running) {
        sleep(30);
        printf("\n[Stats] TX: %lu  RX: %lu\n", 
               app.frames_sent, app.frames_received);
    }
    
    // Wait for threads
    pthread_join(app.tx_thread, NULL);
    pthread_join(app.rx_thread, NULL);
    
    // Print final stats
    printf("\n╔═══════════════════════════════╗\n");
    printf("║      Final Statistics        ║\n");
    printf("╠═══════════════════════════════╣\n");
    printf("║  Frames sent:     %-10lu ║\n", app.frames_sent);
    printf("║  Frames received: %-10lu ║\n", app.frames_received);
    printf("╚═══════════════════════════════╝\n\n");
    
cleanup:
    network_cleanup(&app.net);
    opus_cleanup(&app.opus);
    dma_cleanup(&app.dma);
    gpio_cleanup(&app.gpio);
    
    printf("Goodbye!\n");
    return 0;
}
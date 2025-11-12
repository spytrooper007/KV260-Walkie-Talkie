#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>

// Initialize network
int network_init(network_ctx_t *ctx, const char *peer_ip) {
    memset(ctx, 0, sizeof(network_ctx_t));
    
    // Create UDP socket
    ctx->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (ctx->sockfd < 0) {
        perror("socket");
        return -1;
    }
    
    // Bind to port for receiving
    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(UDP_PORT);
    bind_addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(ctx->sockfd, (struct sockaddr *)&bind_addr, 
             sizeof(bind_addr)) < 0) {
        perror("bind");
        close(ctx->sockfd);
        return -1;
    }
    
    // Setup peer address for sending
    memset(&ctx->peer_addr, 0, sizeof(ctx->peer_addr));
    ctx->peer_addr.sin_family = AF_INET;
    ctx->peer_addr.sin_port = htons(UDP_PORT);
    
    if (inet_pton(AF_INET, peer_ip, &ctx->peer_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid peer IP address: %s\n", peer_ip);
        close(ctx->sockfd);
        return -1;
    }
    
    ctx->initialized = true;
    
    printf("Network initialized: peer=%s:%d\n", peer_ip, UDP_PORT);
    
    return 0;
}

// Send Opus packet to peer
int network_send(network_ctx_t *ctx, const uint8_t *data, uint16_t size) {
    if (!ctx->initialized) {
        fprintf(stderr, "Network not initialized\n");
        return -1;
    }
    
    ssize_t sent = sendto(ctx->sockfd, data, size, 0,
                         (struct sockaddr *)&ctx->peer_addr,
                         sizeof(ctx->peer_addr));
    
    if (sent < 0) {
        perror("sendto");
        return -1;
    }
    
    return sent;
}

// Receive Opus packet (with timeout)
int network_recv(network_ctx_t *ctx, uint8_t *data, int timeout_ms) {
    if (!ctx->initialized) {
        fprintf(stderr, "Network not initialized\n");
        return -1;
    }
    
    // Set receive timeout
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    if (setsockopt(ctx->sockfd, SOL_SOCKET, SO_RCVTIMEO,
                   &tv, sizeof(tv)) < 0) {
        perror("setsockopt");
        return -1;
    }
    
    // Receive packet
    ssize_t received = recvfrom(ctx->sockfd, data, MAX_OPUS_PACKET,
                               0, NULL, NULL);
    
    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;  // Timeout
        }
        perror("recvfrom");
        return -1;
    }
    
    return received;
}

// Cleanup
void network_cleanup(network_ctx_t *ctx) {
    if (ctx->initialized) {
        if (ctx->sockfd >= 0) {
            close(ctx->sockfd);
        }
        ctx->initialized = false;
    }
}
#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>

#define UDP_PORT            5000
#define MAX_OPUS_PACKET     4000

// Network context
typedef struct {
    int sockfd;
    struct sockaddr_in peer_addr;
    bool initialized;
} network_ctx_t;

// Initialize network (UDP socket)
int network_init(network_ctx_t *ctx, const char *peer_ip);

// Send Opus packet to peer
int network_send(network_ctx_t *ctx, const uint8_t *data, uint16_t size);

// Receive Opus packet from peer (non-blocking with timeout)
int network_recv(network_ctx_t *ctx, uint8_t *data, int timeout_ms);

// Cleanup
void network_cleanup(network_ctx_t *ctx);

#endif // NETWORK_H
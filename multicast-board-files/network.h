#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>

// Network configuration
#define MULTICAST_ADDR      "239.0.0.1"
#define MULTICAST_PORT      5000
#define MAX_OPUS_PACKET     4000

// Packet structure
typedef struct __attribute__((packed)) {
    uint32_t board_id;
    uint32_t seq_num;
    uint32_t timestamp_sec;
    uint32_t timestamp_usec;
    uint16_t opus_size;
    uint8_t  flags;
    uint8_t  reserved;
    uint8_t  opus_data[MAX_OPUS_PACKET];
} network_packet_t;

// Packet flags
#define PKT_FLAG_START      0x01
#define PKT_FLAG_END        0x02
#define PKT_FLAG_PRIORITY   0x04

// Network context
typedef struct {
    int sockfd;
    struct sockaddr_in multicast_addr;
    uint32_t my_board_id;
    uint32_t tx_seq_num;
    bool initialized;
} network_ctx_t;

// Initialize network (create socket, join multicast)
int network_init(network_ctx_t *ctx, uint32_t board_id);

// Send Opus packet
int network_send(network_ctx_t *ctx,
                 const uint8_t *opus_data,
                 uint16_t opus_size,
                 uint8_t flags);

// Receive packet (non-blocking with timeout)
int network_recv(network_ctx_t *ctx,
                 network_packet_t *packet,
                 int timeout_ms);

// Cleanup network
void network_cleanup(network_ctx_t *ctx);

// Utility: Get board ID from IP or file
uint32_t network_get_board_id(void);

#endif // NETWORK_H
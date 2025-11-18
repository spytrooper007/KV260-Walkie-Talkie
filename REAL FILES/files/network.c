#define _GNU_SOURCE 
#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <sys/time.h>

// Initialize UDP multicast network
int network_init(network_ctx_t *ctx, uint32_t board_id) {

    // Clear the context structure
    memset(ctx, 0, sizeof(network_ctx_t));
    ctx->my_board_id = board_id;
    ctx->tx_seq_num = 0;

    // Create the UDP socket (SOCK_DGRAM is for UDP)
    ctx->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (ctx->sockfd < 0) return -1;

    // Multiple sockets can use the same PORT number which is needed for multicast
    int reuse = 1;
    setsockopt(ctx->sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));


    // Bind to port so that the OS knows to deliver packets for this port to our socket
    struct sockaddr_in bind_addr = {0};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(MULTICAST_PORT);
    bind_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(ctx->sockfd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) return -1;


    // Join multicast group where MULTICAST_ADDR is the multicast address
    struct ip_mreq mreq;

    // inet_addr takes a string IP address and converts it to binary form
    mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_ADDR);

    // INADDR_ANY means to use the default network interface
    mreq.imr_interface.s_addr = INADDR_ANY;

    // IPPROTO_IP specifies that the option is for IP level
    // IPADD_MEMBERSHIP tells the OS to join the multicast group
    setsockopt(ctx->sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

    // Setup for the multicast address structure
    memset(&ctx->multicast_addr, 0, sizeof(ctx->multicast_addr));

    // AF_INET is for IPv4
    ctx->multicast_addr.sin_family = AF_INET;

    // htons converts from host byte order to network byte order (honestly no clue why this is needed)
    ctx->multicast_addr.sin_port = htons(MULTICAST_PORT);

    // inet_pton converts the string IP address to binary form
    inet_pton(AF_INET, MULTICAST_ADDR, &ctx->multicast_addr.sin_addr);

    ctx->initialized = true;
    printf("Network initialised: %s:%d (Board ID: %u)\n", MULTICAST_ADDR, MULTICAST_PORT, board_id);
    return 0;
}

// Send Opus packet
int network_send(network_ctx_t *ctx, const uint8_t *opus_data, uint16_t opus_size, uint8_t flags) {
    if (!ctx->initialized || opus_size > MAX_OPUS_PACKET) return -1;

    // Prepare the packet with header and Opus data
    network_packet_t packet = {0};
    packet.board_id = ctx->my_board_id;
    packet.seq_num = ctx->tx_seq_num++;
    packet.opus_size = opus_size;
    packet.flags = flags;

    // Copy Opus data into packet
    if (opus_data && opus_size > 0)
        memcpy(packet.opus_data, opus_data, opus_size);

    // Send the packet
    size_t packet_size = sizeof(packet) - MAX_OPUS_PACKET + opus_size;
    return sendto(ctx->sockfd, &packet, packet_size, 0,
                  (struct sockaddr *)&ctx->multicast_addr, sizeof(ctx->multicast_addr));
}

// Receive packet with optional timeout (ms)
int network_recv(network_ctx_t *ctx, network_packet_t *packet, int timeout_ms) {
    if (!ctx->initialized) return -1;

    // Set timeout for receiving
    struct timeval tv = {timeout_ms / 1000, (timeout_ms % 1000) * 1000};
    setsockopt(ctx->sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // Receive packet and return number of bytes received
    ssize_t r = recvfrom(ctx->sockfd, packet, sizeof(network_packet_t), 0, NULL, NULL);
    if (r < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0; // timeout
        return -1;
    }
    return r;
}

// Cleanup network
void network_cleanup(network_ctx_t *ctx) {
    if (ctx->initialized) {
        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_ADDR);
        mreq.imr_interface.s_addr = INADDR_ANY;

        // IP_DROP_MEMBERSHIP to leave the multicast group
        setsockopt(ctx->sockfd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
        close(ctx->sockfd);
        ctx->initialized = false;
    }
}

// Simple board ID (env, file, or default)
uint32_t network_get_board_id(void) {
    const char *env = getenv("BOARD_ID");
    if (env) return (uint32_t)atoi(env);

    FILE *f = fopen("/etc/board_id", "r");
    if (f) { int id; if (fscanf(f, "%d", &id) == 1) { fclose(f); return id; } fclose(f); }

    return 1; // default
}

#pragma once

/** Portable SPI protocol and routing policy; no ESP-IDF or heap dependencies.
 * The wire contract is docs/spi-protocol.md. C structs are never sent directly.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GW_WIRE_VERSION 2u
#define GW_FRAME_SIZE 1600u
#define GW_HEADER_SIZE 40u
#define GW_ETH_HEADER_SIZE 14u
#define GW_IP_MTU 1500u
#define GW_ETH_MAX (GW_ETH_HEADER_SIZE + GW_IP_MTU)
#define GW_STATUS_SIZE 56u
#define GW_MORE 1u
#define GW_TIMEOUT_MS 2000u

enum gw_type
{
    GW_HELLO = 1,
    GW_STATUS,
    GW_DATA,
    GW_IDLE
};
enum gw_status_flag
{
    GW_ASSOCIATED = 1,
    GW_IPV4 = 2,
    GW_FORWARDING = 4,
    GW_OVERLAP = 8,
    GW_CONFIG_ERROR = 16,
};

/** Decoded frame view. payload borrows the input buffer; it is not allocated. */
typedef struct
{
    uint8_t type;
    uint16_t flags;
    uint16_t length;
    uint32_t sequence;
    uint64_t sender;
    uint64_t peer;
    const uint8_t *payload;
} gw_frame_t;

/** One endpoint's handshake state, owned by its transport task. */
typedef struct
{
    uint64_t local;
    uint64_t peer;
    uint32_t received_sequence;
    uint64_t last_rx_ms;
    bool ready; /**< Peer has echoed our current local session identifier. */
} gw_session_t;

typedef enum
{
    GW_REJECT,
    GW_ACCEPT,
    GW_PEER_CHANGED, /**< Caller must close the data gate and rotate its session. */
} gw_accept_t;

/** Diagnostic snapshot. Addresses are network-order octets; counters are host order. */
typedef struct
{
    uint32_t flags;
    uint8_t ip[4];
    uint8_t mask[4];
    uint8_t gateway[4];
    uint8_t dns[2][4];
    uint8_t link_local[4];
    uint8_t link_peer[4];
    uint8_t link_mask[4];
    uint32_t reason;
    uint32_t retry_ms;
    uint32_t received;
    uint32_t rejected;
    uint32_t dropped;
} gw_status_t;

enum gw_ingress
{
    GW_INGRESS_LOCAL,
    GW_INGRESS_SPI,
    GW_INGRESS_UPLINK
};

/** Context for the pre-NAT IPv4 filter. All integer IPv4 addresses are host order. */
typedef struct
{
    enum gw_ingress ingress;
    bool forwarding_ready;
    uint32_t peer_ipv4;
    uint32_t link_ipv4;
    uint32_t link_mask;
} gw_ipv4_policy_t;

/** Standard CRC-32/ISO-HDLC; not an authentication mechanism. */
uint32_t gw_crc32(const uint8_t *data, size_t length);

/** Validate and serialize one frame, including zero padding and checksum. */
bool gw_encode(uint8_t output[GW_FRAME_SIZE], const gw_frame_t *frame);

/** Validate size, checksum and message shape. On success, payload points into data. */
bool gw_decode(const uint8_t *data, size_t length, gw_frame_t *frame);

/** Check Ethernet framing and the supported IPv4/ARP payload shapes. */
bool gw_ethernet_valid(const uint8_t *data, size_t length);

/** Forget the previous pairing. Caller supplies a fresh nonzero local ID. */
void gw_session_init(gw_session_t *session, uint64_t local_session);

/** Process an already-decoded frame. Rejected traffic never refreshes liveness. */
gw_accept_t gw_session_accept(gw_session_t *session, const gw_frame_t *frame, uint64_t now_ms);
bool gw_session_expired(const gw_session_t *session, uint64_t now_ms);

void gw_status_encode(uint8_t output[GW_STATUS_SIZE], const gw_status_t *status);
/** Input must be the payload of a successfully decoded STATUS frame. */
void gw_status_decode(gw_status_t *status, const uint8_t input[GW_STATUS_SIZE]);

/** Address arguments must all use the same byte order; masks must be contiguous. */
bool gw_subnets_overlap(uint32_t address_a, uint32_t mask_a, uint32_t address_b, uint32_t mask_b);
bool gw_can_forward(bool peer_ready, bool ipv4_ready, bool overlap, bool resetting);
/** Host-order addresses: require distinct usable hosts within one /30. */
bool gw_link_addresses_valid(uint32_t local, uint32_t peer, uint32_t mask);

/** Validate unfragmented IPv4 TCP/UDP/ICMP lengths before the SDK's NAPT parser. */
bool gw_ip4_packet_valid(const uint8_t *packet, size_t contiguous_length, size_t total_length);

/** Validate a packet before NAPT reads its transport header.
 * contiguous_length describes the first pbuf segment, total_length the full chain.
 * This function does not free the packet or modify routing state.
 */
bool gw_ip4_allowed(const uint8_t *packet, size_t contiguous_length, size_t total_length,
                    const gw_ipv4_policy_t *policy);

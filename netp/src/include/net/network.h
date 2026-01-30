#ifndef NETWORK_H
#define NETWORK_H

#include "../../lib/types.h"

typedef struct {
    uint8_t dest_mac[6];
    uint8_t src_mac[6];
    uint16_t ethertype;
    uint8_t payload[];
} __attribute__((packed)) ethernet_frame_t;

#define ETHERTYPE_IPV4 0x0800
#define ETHERTYPE_ARP  0x0806
#define ETHERTYPE_IPV6 0x86DD

typedef struct {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t hw_size;
    uint8_t proto_size;
    uint16_t opcode;
    uint8_t sender_mac[6];
    uint32_t sender_ip;
    uint8_t target_mac[6];
    uint32_t target_ip;
} __attribute__((packed)) arp_packet_t;

#define ARP_REQUEST 1
#define ARP_REPLY   2

typedef struct {
    uint8_t version_ihl;
    uint8_t tos;
    uint16_t total_length;
    uint16_t identification;
    uint16_t flags_fragment;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dest_ip;
} __attribute__((packed)) ipv4_header_t;

#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP  6
#define IP_PROTO_UDP  17

typedef struct {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t identifier;
    uint16_t sequence;
} __attribute__((packed)) icmp_header_t;

#define ICMP_ECHO_REPLY   0
#define ICMP_ECHO_REQUEST 8

typedef struct {
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed)) udp_header_t;

typedef struct {
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t data_offset_reserved;
    uint8_t flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent_ptr;
} __attribute__((packed)) tcp_header_t;

#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10
#define TCP_URG 0x20

typedef struct {
    uint16_t transaction_id;
    uint16_t flags;
    uint16_t questions;
    uint16_t answer_rrs;
    uint16_t authority_rrs;
    uint16_t additional_rrs;
} __attribute__((packed)) dns_header_t;

#define DNS_PORT 53
#define DHCP_CLIENT_PORT 68
#define DHCP_SERVER_PORT 67

typedef struct dhcp_packet {
    uint8_t op;
    uint8_t htype;
    uint8_t hlen;
    uint8_t hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint32_t ciaddr;
    uint32_t yiaddr;
    uint32_t siaddr;
    uint32_t giaddr;
    uint8_t chaddr[16];
    uint8_t sname[64];
    uint8_t file[128];
    uint32_t magic_cookie;
    uint8_t options[312];
} __attribute__((packed)) dhcp_packet_t;

#define DHCP_DISCOVER 1
#define DHCP_OFFER    2
#define DHCP_REQUEST  3
#define DHCP_ACK      5

typedef struct arp_entry {
    uint32_t ip;
    uint8_t mac[6];
    uint32_t timestamp;
    uint8_t valid;
    struct arp_entry* next;
} arp_entry_t;

typedef struct nat_entry {
    uint32_t internal_ip;
    uint32_t external_ip;
    uint16_t internal_port;
    uint16_t external_port;
    uint8_t protocol;
    uint32_t timestamp;
    uint8_t valid;
    struct nat_entry* next;
} nat_entry_t;

typedef struct tcp_connection {
    uint32_t local_ip;
    uint32_t remote_ip;
    uint16_t local_port;
    uint16_t remote_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t state;
    uint32_t timestamp;
    struct tcp_connection* next;
} tcp_connection_t;

#define TCP_STATE_CLOSED      0
#define TCP_STATE_LISTEN      1
#define TCP_STATE_SYN_SENT    2
#define TCP_STATE_SYN_RECEIVED 3
#define TCP_STATE_ESTABLISHED 4
#define TCP_STATE_FIN_WAIT1   5
#define TCP_STATE_FIN_WAIT2   6
#define TCP_STATE_CLOSE_WAIT  7
#define TCP_STATE_CLOSING     8
#define TCP_STATE_LAST_ACK    9
#define TCP_STATE_TIME_WAIT   10

typedef struct {
    uint32_t ip_addr;
    uint32_t subnet_mask;
    uint32_t gateway;
    uint32_t dns_server;
    uint8_t mac_addr[6];
    uint8_t dhcp_enabled;
    uint8_t nat_enabled;
} net_config_t;

typedef struct {
    uint32_t rx_packets;
    uint32_t tx_packets;
    uint32_t rx_bytes;
    uint32_t tx_bytes;
    uint32_t rx_errors;
    uint32_t tx_errors;
    uint32_t rx_dropped;
    uint32_t tx_dropped;
} net_stats_t;

void network_init(void);
void network_process_packet(const uint8_t* data, uint32_t len);
int network_send_ethernet(const uint8_t* dest_mac, uint16_t ethertype, const void* payload, uint32_t len);
int network_send_ip(uint32_t dest_ip, uint8_t protocol, const void* payload, uint32_t len);
int network_send_udp(uint32_t dest_ip, uint16_t src_port, uint16_t dest_port, const void* data, uint32_t len);
int network_send_tcp(uint32_t dest_ip, uint16_t src_port, uint16_t dest_port, uint8_t flags, uint32_t seq, uint32_t ack, const void* data, uint32_t len);
void network_send_arp_request(uint32_t target_ip);
void network_send_ping(uint32_t dest_ip, uint16_t seq);
uint8_t* network_arp_lookup(uint32_t ip);
void network_set_ip(uint32_t ip, uint32_t mask, uint32_t gateway);
void network_get_config(net_config_t* config);
void network_get_stats(net_stats_t* stats);
void network_print_stats(void);
uint16_t network_checksum(const void* data, uint32_t len);
void network_enable_nat(uint8_t enable);
void network_dhcp_request(void);
int network_dns_resolve(const char* hostname, uint32_t* ip);
tcp_connection_t* network_tcp_connect(uint32_t dest_ip, uint16_t dest_port);
int network_tcp_send(tcp_connection_t* conn, const void* data, uint32_t len);
int network_tcp_close(tcp_connection_t* conn);
int network_http_get(const char* hostname, const char* path, char* response, uint32_t max_len);
uint32_t ip_from_bytes(uint8_t a, uint8_t b, uint8_t c, uint8_t d);
void ip_to_string(uint32_t ip, char* str);
uint32_t ip_from_string(const char* str);

#endif

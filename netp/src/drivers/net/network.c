#include "../../include/net/network.h"
#include "../../include/net/rtl8139.h"
#include "../../include/text.h"
#include "../../include/memory/pmm.h"
#include "../../lib/string.h"
#include "../../lib/time.h"

static net_config_t net_config;
static net_stats_t net_stats;
static arp_entry_t* arp_cache_head = NULL;
static nat_entry_t* nat_table_head = NULL;
static tcp_connection_t* tcp_connections_head = NULL;
static uint16_t next_source_port = 49152;

static inline uint16_t htons(uint16_t hostshort) {
    return ((hostshort & 0xFF) << 8) | ((hostshort >> 8) & 0xFF);
}

static inline uint32_t htonl(uint32_t hostlong) {
    return ((hostlong & 0xFF) << 24) | ((hostlong & 0xFF00) << 8) |
           ((hostlong & 0xFF0000) >> 8) | ((hostlong >> 24) & 0xFF);
}

#define ntohs htons
#define ntohl htonl

uint16_t network_checksum(const void* data, uint32_t len) {
    const uint16_t* buf = (const uint16_t*)data;
    uint32_t sum = 0;
    
    while (len > 1) {
        sum += *buf++;
        len -= 2;
    }
    
    if (len > 0) {
        sum += *(uint8_t*)buf;
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return ~sum;
}

uint32_t ip_from_bytes(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return (a << 24) | (b << 16) | (c << 8) | d;
}

void ip_to_string(uint32_t ip, char* str) {
    uint8_t* bytes = (uint8_t*)&ip;
    sprintf(str, "%u.%u.%u.%u", bytes[3], bytes[2], bytes[1], bytes[0]);
}

uint32_t ip_from_string(const char* str) {
    uint32_t a = 0, b = 0, c = 0, d = 0;
    const char* p = str;
    a = 0; while (*p >= '0' && *p <= '9') { a = a * 10 + (*p++ - '0'); }
    if (*p++ != '.') return 0;
    b = 0; while (*p >= '0' && *p <= '9') { b = b * 10 + (*p++ - '0'); }
    if (*p++ != '.') return 0;
    c = 0; while (*p >= '0' && *p <= '9') { c = c * 10 + (*p++ - '0'); }
    if (*p++ != '.') return 0;
    d = 0; while (*p >= '0' && *p <= '9') { d = d * 10 + (*p++ - '0'); }
    return ip_from_bytes(a, b, c, d);
}

void network_init(void) {
    memset(&net_config, 0, sizeof(net_config_t));
    memset(&net_stats, 0, sizeof(net_stats_t));
    
    rtl8139_get_mac_address(net_config.mac_addr);
    
    net_config.ip_addr = ip_from_bytes(10, 0, 2, 15);
    net_config.subnet_mask = ip_from_bytes(255, 255, 255, 0);
    net_config.gateway = ip_from_bytes(10, 0, 2, 2);
    net_config.dns_server = ip_from_bytes(8, 8, 8, 8);
    net_config.nat_enabled = 1;
    net_config.dhcp_enabled = 0;
    
    printf("{FG(0,255,0)}Network stack initialized with NAT support\n");
}

static arp_entry_t* arp_cache_find(uint32_t ip) {
    arp_entry_t* entry = arp_cache_head;
    while (entry) {
        if (entry->valid && entry->ip == ip) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

static void arp_cache_add(uint32_t ip, const uint8_t* mac) {
    arp_entry_t* entry = arp_cache_find(ip);
    
    if (!entry) {
        entry = (arp_entry_t*)pmm_malloc(sizeof(arp_entry_t));
        if (!entry) return;
        entry->next = arp_cache_head;
        arp_cache_head = entry;
    }
    
    entry->ip = ip;
    memcpy(entry->mac, mac, 6);
    entry->timestamp = get_ticks();
    entry->valid = 1;
}

uint8_t* network_arp_lookup(uint32_t ip) {
    arp_entry_t* entry = arp_cache_find(ip);
    return entry ? entry->mac : NULL;
}

static nat_entry_t* nat_find_entry(uint32_t internal_ip, uint16_t internal_port, uint8_t protocol) {
    nat_entry_t* entry = nat_table_head;
    while (entry) {
        if (entry->valid && entry->internal_ip == internal_ip && 
            entry->internal_port == internal_port && entry->protocol == protocol) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

static nat_entry_t* nat_create_entry(uint32_t internal_ip, uint16_t internal_port, uint8_t protocol) {
    nat_entry_t* entry = (nat_entry_t*)pmm_malloc(sizeof(nat_entry_t));
    if (!entry) return NULL;
    
    entry->internal_ip = internal_ip;
    entry->external_ip = net_config.ip_addr;
    entry->internal_port = internal_port;
    entry->external_port = next_source_port++;
    if (next_source_port > 65535) next_source_port = 49152;
    entry->protocol = protocol;
    entry->timestamp = get_ticks();
    entry->valid = 1;
    entry->next = nat_table_head;
    nat_table_head = entry;
    
    return entry;
}

static void handle_arp_packet(const uint8_t* data, uint32_t len) {
    if (len < sizeof(arp_packet_t)) return;
    
    arp_packet_t* arp = (arp_packet_t*)data;
    
    if (ntohs(arp->hw_type) != 1 || ntohs(arp->proto_type) != ETHERTYPE_IPV4) {
        return;
    }
    
    uint32_t sender_ip = ntohl(arp->sender_ip);
    uint32_t target_ip = ntohl(arp->target_ip);
    
    arp_cache_add(sender_ip, arp->sender_mac);
    
    if (ntohs(arp->opcode) == ARP_REQUEST && target_ip == net_config.ip_addr) {
        uint8_t reply_buf[sizeof(ethernet_frame_t) + sizeof(arp_packet_t)];
        ethernet_frame_t* eth = (ethernet_frame_t*)reply_buf;
        arp_packet_t* reply = (arp_packet_t*)eth->payload;
        
        memcpy(eth->dest_mac, arp->sender_mac, 6);
        memcpy(eth->src_mac, net_config.mac_addr, 6);
        eth->ethertype = htons(ETHERTYPE_ARP);
        
        reply->hw_type = htons(1);
        reply->proto_type = htons(ETHERTYPE_IPV4);
        reply->hw_size = 6;
        reply->proto_size = 4;
        reply->opcode = htons(ARP_REPLY);
        memcpy(reply->sender_mac, net_config.mac_addr, 6);
        reply->sender_ip = htonl(net_config.ip_addr);
        memcpy(reply->target_mac, arp->sender_mac, 6);
        reply->target_ip = arp->sender_ip;
        
        rtl8139_send_packet(reply_buf, sizeof(reply_buf));
        net_stats.tx_packets++;
    }
}

static tcp_connection_t* tcp_find_connection(uint32_t remote_ip, uint16_t local_port, uint16_t remote_port) {
    tcp_connection_t* conn = tcp_connections_head;
    while (conn) {
        if (conn->remote_ip == remote_ip && conn->local_port == local_port && 
            conn->remote_port == remote_port) {
            return conn;
        }
        conn = conn->next;
    }
    return NULL;
}

static void handle_tcp_packet(const uint8_t* data, uint32_t len, uint32_t src_ip, uint32_t dest_ip) {
    if (len < sizeof(tcp_header_t)) return;
    
    tcp_header_t* tcp = (tcp_header_t*)data;
    uint16_t src_port = ntohs(tcp->src_port);
    uint16_t dest_port = ntohs(tcp->dest_port);
    uint32_t seq = ntohl(tcp->seq_num);
    uint32_t ack = ntohl(tcp->ack_num);
    uint8_t flags = tcp->flags;
    
    tcp_connection_t* conn = tcp_find_connection(src_ip, dest_port, src_port);
    
    if (flags & TCP_SYN) {
        if (!conn) {
            conn = (tcp_connection_t*)pmm_malloc(sizeof(tcp_connection_t));
            if (!conn) return;
            conn->local_ip = dest_ip;
            conn->remote_ip = src_ip;
            conn->local_port = dest_port;
            conn->remote_port = src_port;
            conn->seq_num = 1000;
            conn->ack_num = seq + 1;
            conn->state = TCP_STATE_SYN_RECEIVED;
            conn->timestamp = get_ticks();
            conn->next = tcp_connections_head;
            tcp_connections_head = conn;
        }
    } else if (conn) {
        conn->ack_num = seq + 1;
        conn->timestamp = get_ticks();
        
        if (flags & TCP_ACK) {
            if (conn->state == TCP_STATE_SYN_RECEIVED) {
                conn->state = TCP_STATE_ESTABLISHED;
            } else if (conn->state == TCP_STATE_FIN_WAIT1) {
                conn->state = TCP_STATE_FIN_WAIT2;
            }
        }
        
        if (flags & TCP_FIN) {
            conn->state = TCP_STATE_CLOSE_WAIT;
        }
    }
}

static void handle_udp_packet(const uint8_t* data, uint32_t len, uint32_t src_ip) {
    if (len < sizeof(udp_header_t)) return;
    
    udp_header_t* udp = (udp_header_t*)data;
    uint16_t src_port = ntohs(udp->src_port);
    uint16_t dest_port = ntohs(udp->dest_port);
    uint16_t udp_len = ntohs(udp->length);
    
    if (dest_port == DNS_PORT) {
    }
}

static void handle_icmp_packet(const uint8_t* data, uint32_t len, uint32_t src_ip) {
    if (len < sizeof(icmp_header_t)) return;
    
    icmp_header_t* icmp = (icmp_header_t*)data;
    
    if (icmp->type == ICMP_ECHO_REQUEST) {
        uint8_t reply_buf[sizeof(ethernet_frame_t) + sizeof(ipv4_header_t) + len];
        ethernet_frame_t* eth = (ethernet_frame_t*)reply_buf;
        ipv4_header_t* ip = (ipv4_header_t*)eth->payload;
        icmp_header_t* reply_icmp = (icmp_header_t*)(ip + 1);
        
        uint8_t* dest_mac = network_arp_lookup(src_ip);
        if (!dest_mac) {
            network_send_arp_request(src_ip);
            return;
        }
        
        memcpy(eth->dest_mac, dest_mac, 6);
        memcpy(eth->src_mac, net_config.mac_addr, 6);
        eth->ethertype = htons(ETHERTYPE_IPV4);
        
        ip->version_ihl = 0x45;
        ip->tos = 0;
        ip->total_length = htons(sizeof(ipv4_header_t) + len);
        ip->identification = 0;
        ip->flags_fragment = 0;
        ip->ttl = 64;
        ip->protocol = IP_PROTO_ICMP;
        ip->checksum = 0;
        ip->src_ip = htonl(net_config.ip_addr);
        ip->dest_ip = htonl(src_ip);
        ip->checksum = network_checksum(ip, sizeof(ipv4_header_t));
        
        memcpy(reply_icmp, icmp, len);
        reply_icmp->type = ICMP_ECHO_REPLY;
        reply_icmp->checksum = 0;
        reply_icmp->checksum = network_checksum(reply_icmp, len);
        
        rtl8139_send_packet(reply_buf, sizeof(reply_buf));
        net_stats.tx_packets++;
    }
}

static void handle_ip_packet(const uint8_t* data, uint32_t len) {
    if (len < sizeof(ipv4_header_t)) return;
    
    ipv4_header_t* ip = (ipv4_header_t*)data;
    
    if ((ip->version_ihl >> 4) != 4) return;
    
    uint32_t header_len = (ip->version_ihl & 0x0F) * 4;
    uint32_t total_len = ntohs(ip->total_length);
    
    if (total_len > len || header_len > total_len) return;
    
    uint32_t dest_ip = ntohl(ip->dest_ip);
    uint32_t src_ip = ntohl(ip->src_ip);
    
    if (dest_ip != net_config.ip_addr && !net_config.nat_enabled) return;
    
    uint8_t* payload = (uint8_t*)ip + header_len;
    uint32_t payload_len = total_len - header_len;
    
    switch (ip->protocol) {
        case IP_PROTO_ICMP:
            handle_icmp_packet(payload, payload_len, src_ip);
            break;
        case IP_PROTO_TCP:
            handle_tcp_packet(payload, payload_len, src_ip, dest_ip);
            break;
        case IP_PROTO_UDP:
            handle_udp_packet(payload, payload_len, src_ip);
            break;
    }
}

void network_process_packet(const uint8_t* data, uint32_t len) {
    if (len < sizeof(ethernet_frame_t)) return;
    
    ethernet_frame_t* eth = (ethernet_frame_t*)data;
    uint16_t ethertype = ntohs(eth->ethertype);
    
    net_stats.rx_packets++;
    net_stats.rx_bytes += len;
    
    switch (ethertype) {
        case ETHERTYPE_ARP:
            handle_arp_packet(eth->payload, len - sizeof(ethernet_frame_t));
            break;
        case ETHERTYPE_IPV4:
            handle_ip_packet(eth->payload, len - sizeof(ethernet_frame_t));
            break;
    }
}

int network_send_ethernet(const uint8_t* dest_mac, uint16_t ethertype, const void* payload, uint32_t len) {
    uint8_t frame[sizeof(ethernet_frame_t) + len];
    ethernet_frame_t* eth = (ethernet_frame_t*)frame;
    
    memcpy(eth->dest_mac, dest_mac, 6);
    memcpy(eth->src_mac, net_config.mac_addr, 6);
    eth->ethertype = htons(ethertype);
    memcpy(eth->payload, payload, len);
    
    int result = rtl8139_send_packet(frame, sizeof(frame));
    if (result == 0) {
        net_stats.tx_packets++;
        net_stats.tx_bytes += sizeof(frame);
    }
    return result;
}

void network_send_arp_request(uint32_t target_ip) {
    arp_packet_t arp;
    
    arp.hw_type = htons(1);
    arp.proto_type = htons(ETHERTYPE_IPV4);
    arp.hw_size = 6;
    arp.proto_size = 4;
    arp.opcode = htons(ARP_REQUEST);
    memcpy(arp.sender_mac, net_config.mac_addr, 6);
    arp.sender_ip = htonl(net_config.ip_addr);
    memset(arp.target_mac, 0, 6);
    arp.target_ip = htonl(target_ip);
    
    uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    network_send_ethernet(broadcast, ETHERTYPE_ARP, &arp, sizeof(arp));
}

int network_send_ip(uint32_t dest_ip, uint8_t protocol, const void* payload, uint32_t len) {
    uint32_t gateway_ip = ((dest_ip & net_config.subnet_mask) == (net_config.ip_addr & net_config.subnet_mask)) 
                          ? dest_ip : net_config.gateway;
    
    uint8_t* dest_mac = network_arp_lookup(gateway_ip);
    if (!dest_mac) {
        network_send_arp_request(gateway_ip);
        return -1;
    }
    
    uint8_t packet[sizeof(ipv4_header_t) + len];
    ipv4_header_t* ip = (ipv4_header_t*)packet;
    
    ip->version_ihl = 0x45;
    ip->tos = 0;
    ip->total_length = htons(sizeof(packet));
    ip->identification = htons(get_ticks() & 0xFFFF);
    ip->flags_fragment = 0;
    ip->ttl = 64;
    ip->protocol = protocol;
    ip->checksum = 0;
    ip->src_ip = htonl(net_config.ip_addr);
    ip->dest_ip = htonl(dest_ip);
    ip->checksum = network_checksum(ip, sizeof(ipv4_header_t));
    
    memcpy(packet + sizeof(ipv4_header_t), payload, len);
    
    return network_send_ethernet(dest_mac, ETHERTYPE_IPV4, packet, sizeof(packet));
}

int network_send_udp(uint32_t dest_ip, uint16_t src_port, uint16_t dest_port, const void* data, uint32_t len) {
    uint8_t packet[sizeof(udp_header_t) + len];
    udp_header_t* udp = (udp_header_t*)packet;
    
    udp->src_port = htons(src_port);
    udp->dest_port = htons(dest_port);
    udp->length = htons(sizeof(packet));
    udp->checksum = 0;
    memcpy(packet + sizeof(udp_header_t), data, len);
    
    return network_send_ip(dest_ip, IP_PROTO_UDP, packet, sizeof(packet));
}

int network_send_tcp(uint32_t dest_ip, uint16_t src_port, uint16_t dest_port, uint8_t flags, 
                     uint32_t seq, uint32_t ack, const void* data, uint32_t len) {
    uint8_t packet[sizeof(tcp_header_t) + len];
    tcp_header_t* tcp = (tcp_header_t*)packet;
    
    tcp->src_port = htons(src_port);
    tcp->dest_port = htons(dest_port);
    tcp->seq_num = htonl(seq);
    tcp->ack_num = htonl(ack);
    tcp->data_offset_reserved = 0x50;
    tcp->flags = flags;
    tcp->window = htons(65535);
    tcp->checksum = 0;
    tcp->urgent_ptr = 0;
    memcpy(packet + sizeof(tcp_header_t), data, len);
    
    return network_send_ip(dest_ip, IP_PROTO_TCP, packet, sizeof(packet));
}

void network_send_ping(uint32_t dest_ip, uint16_t seq) {
    uint8_t* dest_mac = network_arp_lookup(dest_ip);
    if (!dest_mac) {
        network_send_arp_request(dest_ip);
        return;
    }
    
    uint8_t packet[sizeof(ipv4_header_t) + sizeof(icmp_header_t) + 32];
    ipv4_header_t* ip = (ipv4_header_t*)packet;
    icmp_header_t* icmp = (icmp_header_t*)(ip + 1);
    uint8_t* icmp_data = (uint8_t*)(icmp + 1);
    
    for (int i = 0; i < 32; i++) {
        icmp_data[i] = 'A' + (i % 26);
    }
    
    icmp->type = ICMP_ECHO_REQUEST;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->identifier = htons(0x1234);
    icmp->sequence = htons(seq);
    icmp->checksum = network_checksum(icmp, sizeof(icmp_header_t) + 32);
    
    ip->version_ihl = 0x45;
    ip->tos = 0;
    ip->total_length = htons(sizeof(packet));
    ip->identification = htons(seq);
    ip->flags_fragment = 0;
    ip->ttl = 64;
    ip->protocol = IP_PROTO_ICMP;
    ip->checksum = 0;
    ip->src_ip = htonl(net_config.ip_addr);
    ip->dest_ip = htonl(dest_ip);
    ip->checksum = network_checksum(ip, sizeof(ipv4_header_t));
    
    network_send_ethernet(dest_mac, ETHERTYPE_IPV4, packet, sizeof(packet));
}

tcp_connection_t* network_tcp_connect(uint32_t dest_ip, uint16_t dest_port) {
    tcp_connection_t* conn = (tcp_connection_t*)pmm_malloc(sizeof(tcp_connection_t));
    if (!conn) return NULL;
    
    conn->local_ip = net_config.ip_addr;
    conn->remote_ip = dest_ip;
    conn->local_port = next_source_port++;
    conn->remote_port = dest_port;
    conn->seq_num = get_ticks();
    conn->ack_num = 0;
    conn->state = TCP_STATE_SYN_SENT;
    conn->timestamp = get_ticks();
    conn->next = tcp_connections_head;
    tcp_connections_head = conn;
    
    network_send_tcp(dest_ip, conn->local_port, dest_port, TCP_SYN, conn->seq_num, 0, NULL, 0);
    conn->seq_num++;
    
    return conn;
}

int network_tcp_send(tcp_connection_t* conn, const void* data, uint32_t len) {
    if (!conn || conn->state != TCP_STATE_ESTABLISHED) return -1;
    
    int result = network_send_tcp(conn->remote_ip, conn->local_port, conn->remote_port, 
                                   TCP_PSH | TCP_ACK, conn->seq_num, conn->ack_num, data, len);
    if (result == 0) {
        conn->seq_num += len;
    }
    return result;
}

int network_tcp_close(tcp_connection_t* conn) {
    if (!conn) return -1;
    
    conn->state = TCP_STATE_FIN_WAIT1;
    return network_send_tcp(conn->remote_ip, conn->local_port, conn->remote_port, 
                            TCP_FIN | TCP_ACK, conn->seq_num, conn->ack_num, NULL, 0);
}

int network_dns_resolve(const char* hostname, uint32_t* ip) {
    return -1;
}

int network_http_get(const char* hostname, const char* path, char* response, uint32_t max_len) {
    uint32_t server_ip;
    if (network_dns_resolve(hostname, &server_ip) != 0) {
        return -1;
    }
    
    tcp_connection_t* conn = network_tcp_connect(server_ip, 80);
    if (!conn) return -1;
    
    uint32_t timeout = get_ticks() + 3000;
    while (conn->state != TCP_STATE_ESTABLISHED && get_ticks() < timeout) {
    }
    
    if (conn->state != TCP_STATE_ESTABLISHED) {
        return -1;
    }
    
    char request[512];
    sprintf(request, "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", path, hostname);
    
    if (network_tcp_send(conn, request, strlen(request)) != 0) {
        network_tcp_close(conn);
        return -1;
    }
    
    network_tcp_close(conn);
    return 0;
}

void network_enable_nat(uint8_t enable) {
    net_config.nat_enabled = enable;
}

void network_dhcp_request(void) {
    dhcp_packet_t dhcp;
    memset(&dhcp, 0, sizeof(dhcp_packet_t));
    
    dhcp.op = 1;
    dhcp.htype = 1;
    dhcp.hlen = 6;
    dhcp.xid = htonl(get_ticks());
    memcpy(dhcp.chaddr, net_config.mac_addr, 6);
    dhcp.magic_cookie = htonl(0x63825363);
    
    dhcp.options[0] = 53;
    dhcp.options[1] = 1;
    dhcp.options[2] = DHCP_DISCOVER;
    dhcp.options[3] = 255;
    
    network_send_udp(ip_from_bytes(255, 255, 255, 255), DHCP_CLIENT_PORT, DHCP_SERVER_PORT, 
                     &dhcp, sizeof(dhcp_packet_t));
}

void network_set_ip(uint32_t ip, uint32_t mask, uint32_t gateway) {
    net_config.ip_addr = ip;
    net_config.subnet_mask = mask;
    net_config.gateway = gateway;
}

void network_get_config(net_config_t* config) {
    if (config) {
        memcpy(config, &net_config, sizeof(net_config_t));
    }
}

void network_get_stats(net_stats_t* stats) {
    if (stats) {
        memcpy(stats, &net_stats, sizeof(net_stats_t));
    }
}

void network_print_stats(void) {
    printf("{FG(255,255,0)}Network Statistics:\n");
    printf("{FG(0,255,255)}RX: {FG(255,255,255)}%u pkts, %u bytes\n", 
           net_stats.rx_packets, net_stats.rx_bytes);
    printf("{FG(0,255,255)}TX: {FG(255,255,255)}%u pkts, %u bytes\n", 
           net_stats.tx_packets, net_stats.tx_bytes);
    printf("{FG(0,255,255)}Errors: {FG(255,255,255)}RX %u, TX %u\n", 
           net_stats.rx_errors, net_stats.tx_errors);
}

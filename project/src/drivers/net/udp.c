#include "../../include/net/udp.h"
#include "../../include/net/ip.h"
#include "../../include/net/eth.h"
#include "../../lib/string.h"
#include "../../lib/types.h"

#define UDP_HANDLERS 8

typedef struct {
    u16           port;
    udp_handler_t handler;
} udp_binding_t;

static udp_binding_t bindings[UDP_HANDLERS];

void udp_register(u16 port, udp_handler_t handler) {
    u8 i;
    for (i = 0; i < UDP_HANDLERS; i++) {
        if (!bindings[i].handler) {
            bindings[i].port    = port;
            bindings[i].handler = handler;
            return;
        }
    }
}

void udp_unregister(u16 port) {
    u8 i;
    for (i = 0; i < UDP_HANDLERS; i++) {
        if (bindings[i].port == port) {
            bindings[i].handler = 0;
            bindings[i].port    = 0;
        }
    }
}

int udp_send(u32 dst_ip, u16 src_port, u16 dst_port, const void *data, u16 len) {
    u8        buf[1500];
    udp_hdr_t *h = (udp_hdr_t*)buf;
    u16 total = sizeof(udp_hdr_t) + len;

    if (total > sizeof(buf)) return -1;

    h->src_port = htons(src_port);
    h->dst_port = htons(dst_port);
    h->len      = htons(total);
    h->chk      = 0;
    memcpy(buf + sizeof(udp_hdr_t), data, len);

    return ip_send(dst_ip, IP_PROTO_UDP, buf, total);
}

void udp_recv(u32 src_ip, const u8 *data, u16 len) {
    udp_hdr_t *h;
    u16 dst_port;
    u8  i;

    if (len < (u16)sizeof(udp_hdr_t)) return;
    h        = (udp_hdr_t*)data;
    dst_port = ntohs(h->dst_port);

    for (i = 0; i < UDP_HANDLERS; i++) {
        if (bindings[i].handler && bindings[i].port == dst_port) {
            bindings[i].handler(src_ip, ntohs(h->src_port),
                                data + sizeof(udp_hdr_t),
                                ntohs(h->len) - sizeof(udp_hdr_t));
            return;
        }
    }
}

#include "../../include/net/ip.h"
#include "../../include/net/eth.h"
#include "../../include/net/arp.h"
#include "../../include/net/icmp.h"
#include "../../lib/string.h"
#include "../../lib/types.h"

static u16 ip_id = 0;

u16 ip_checksum(const void *data, u16 len) {
    const u16 *p = (const u16*)data;
    u32 s = 0;
    while (len > 1) { s += *p++; len -= 2; }
    if (len) s += *(u8*)p;
    while (s >> 16) s = (s & 0xFFFF) + (s >> 16);
    return (u16)~s;
}

int ip_send(u32 dst, u8 proto, const void *data, u16 len) {
    u8 buf[ETH_FRAME_MAX];
    ip_hdr_t *h = (ip_hdr_t*)buf;
    u8 dst_mac[ETH_ALEN];
    u16 tot;

    tot = sizeof(ip_hdr_t) + len;
    if (tot > ETH_FRAME_MAX) return -1;

    if (arp_resolve(dst, dst_mac) < 0) return -1;

    h->ihl_ver  = 0x45;
    h->tos      = 0;
    h->tot_len  = htons(tot);
    h->id       = htons(ip_id++);
    h->frag_off = 0;
    h->ttl      = 64;
    h->proto    = proto;
    h->chk      = 0;
    h->src      = arp_get_ip();
    h->dst      = dst;
    h->chk      = ip_checksum(h, sizeof(ip_hdr_t));

    memcpy(buf + sizeof(ip_hdr_t), data, len);
    return eth_send(dst_mac, ETH_TYPE_IP, buf, tot);
}

void ip_recv(const u8 *data, u16 len) {
    ip_hdr_t *h;
    u8 ihl;

    if (len < (u16)sizeof(ip_hdr_t)) return;
    h   = (ip_hdr_t*)data;
    ihl = (h->ihl_ver & 0x0F) * 4;
    if (ihl < 20 || ihl > len) return;

    switch (h->proto) {
    case IP_PROTO_ICMP:
        icmp_recv(h->src, data + ihl, len - ihl);
        break;
    default:
        break;
    }
}

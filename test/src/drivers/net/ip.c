#include "../../include/net/ip.h"
#include "../../include/net/eth.h"
#include "../../include/net/arp.h"
#include "../../include/net/icmp.h"
#include "../../include/net/udp.h"
#include "../../include/net/tcp.h"
#include "../../lib/string.h"
#include "../../lib/types.h"
#include "../../include/text.h"

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
    u8  dst_mac[ETH_ALEN];
    u16 tot;
    u32 my_ip   = arp_get_ip();
    u32 netmask = arp_get_netmask();
    u32 gateway = arp_get_gateway();
    u32 next_hop;

    tot = sizeof(ip_hdr_t) + len;
    if (tot > ETH_FRAME_MAX) return -1;

    if ((dst & netmask) == (my_ip & netmask))
        next_hop = dst;
    else
        next_hop = gateway;

    printf("[IP] send: dst=%u.%u.%u.%u next_hop=%u.%u.%u.%u proto=%u\n",
        (dst>>24)&0xFF,(dst>>16)&0xFF,(dst>>8)&0xFF,dst&0xFF,
        (next_hop>>24)&0xFF,(next_hop>>16)&0xFF,(next_hop>>8)&0xFF,next_hop&0xFF,(unsigned)proto);
    printf("[IP] send: my_ip=%u.%u.%u.%u mask=0x%08x gw=%u.%u.%u.%u\n",
        (my_ip>>24)&0xFF,(my_ip>>16)&0xFF,(my_ip>>8)&0xFF,my_ip&0xFF,
        (unsigned)netmask,
        (gateway>>24)&0xFF,(gateway>>16)&0xFF,(gateway>>8)&0xFF,gateway&0xFF);
    if (arp_resolve(next_hop, dst_mac) < 0) {
        printf("[IP] send: arp_resolve FAILED for %u.%u.%u.%u\n",
            (next_hop>>24)&0xFF,(next_hop>>16)&0xFF,(next_hop>>8)&0xFF,next_hop&0xFF);
        return -1;
    }

    h->ihl_ver  = 0x45;
    h->tos      = 0;
    h->tot_len  = htons(tot);
    h->id       = htons(ip_id++);
    h->frag_off = 0;
    h->ttl      = 64;
    h->proto    = proto;
    h->chk      = 0;
    h->src      = htonl(my_ip);
    h->dst      = htonl(dst);
    h->chk      = ip_checksum(h, sizeof(ip_hdr_t));

    memcpy(buf + sizeof(ip_hdr_t), data, len);
    int r = eth_send(dst_mac, ETH_TYPE_IP, buf, tot);
    printf("[IP] send proto=%d dst=%u.%u.%u.%u next=%u.%u.%u.%u ret=%d\n",
        (int)proto,
        (dst>>24)&0xFF,(dst>>16)&0xFF,(dst>>8)&0xFF,dst&0xFF,
        (next_hop>>24)&0xFF,(next_hop>>16)&0xFF,(next_hop>>8)&0xFF,next_hop&0xFF,
        r);
    return r;
}

void ip_recv(const u8 *data, u16 len) {
    ip_hdr_t *h;
    u8 ihl;

    if (len < (u16)sizeof(ip_hdr_t)) return;
    h   = (ip_hdr_t*)data;
    ihl = (h->ihl_ver & 0x0F) * 4;
    if (ihl < 20 || ihl > len) return;

    printf("[IP] recv proto=%d src=%u.%u.%u.%u dst=%u.%u.%u.%u len=%u\n",
        (int)h->proto,
        (ntohl(h->src)>>24)&0xFF,(ntohl(h->src)>>16)&0xFF,
        (ntohl(h->src)>>8)&0xFF,ntohl(h->src)&0xFF,
        (ntohl(h->dst)>>24)&0xFF,(ntohl(h->dst)>>16)&0xFF,
        (ntohl(h->dst)>>8)&0xFF,ntohl(h->dst)&0xFF,
        (unsigned)ntohs(h->tot_len));

    switch (h->proto) {
    case IP_PROTO_ICMP:
        icmp_recv(ntohl(h->src), data + ihl, len - ihl);
        break;
    case IP_PROTO_UDP:
        udp_recv(ntohl(h->src), data + ihl, len - ihl);
        break;
    case IP_PROTO_TCP:
        tcp_recv_packet(ntohl(h->src), data + ihl, len - ihl);
        break;
    default:
        break;
    }
}

#include "../../include/net/arp.h"
#include "../../include/net/eth.h"
#include "../../include/net/ne2000.h"
#include "../../include/timer/pit.h"
#include "../../lib/string.h"
#include "../../lib/types.h"

#define ARP_CACHE 16

typedef struct {
    u32 ip;
    u8  mac[ETH_ALEN];
    u8  valid;
} arp_entry_t;

static arp_entry_t cache[ARP_CACHE];
static u32 my_ip   = 0;
static u32 my_mask = 0;
static u32 my_gw   = 0;
static u8  bcast[ETH_ALEN] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

void arp_set_ip(u32 ip) { my_ip = ip; }
u32  arp_get_ip(void)      { return my_ip; }
u32  arp_get_netmask(void) { return my_mask; }
u32  arp_get_gateway(void) { return my_gw; }

void arp_init(u32 ip, u32 netmask, u32 gateway) {
    my_ip   = ip;
    my_mask = netmask;
    my_gw   = gateway;
    memset(cache, 0, sizeof(cache));
}

static void cache_put(u32 ip, const u8 *mac) {
    u8 i;
    for (i = 0; i < ARP_CACHE; i++) {
        if (!cache[i].valid || cache[i].ip == ip) {
            cache[i].ip = ip;
            memcpy(cache[i].mac, mac, ETH_ALEN);
            cache[i].valid = 1;
            return;
        }
    }
    cache[0].ip = ip;
    memcpy(cache[0].mac, mac, ETH_ALEN);
    cache[0].valid = 1;
}

static int cache_get(u32 ip, u8 *mac) {
    u8 i;
    for (i = 0; i < ARP_CACHE; i++) {
        if (cache[i].valid && cache[i].ip == ip) {
            memcpy(mac, cache[i].mac, ETH_ALEN);
            return 0;
        }
    }
    return -1;
}

static void arp_send(u16 op, const u8 *tha, u32 tpa) {
    arp_pkt_t p;
    u8 my_mac[ETH_ALEN];
    eth_get_mac(my_mac);

    p.htype = htons(1);
    p.ptype = htons(ETH_TYPE_IP);
    p.hlen  = ETH_ALEN;
    p.plen  = 4;
    p.op    = htons(op);
    memcpy(p.sha, my_mac, ETH_ALEN);
    p.spa   = htonl(my_ip);
    memcpy(p.tha, tha, ETH_ALEN);
    p.tpa   = htonl(tpa);

    eth_send(op == ARP_OP_REQUEST ? bcast : tha, ETH_TYPE_ARP, &p, sizeof(p));
}

void arp_recv(const u8 *data, u16 len) {
    arp_pkt_t *p;
    u32 spa, tpa;

    if (len < (u16)sizeof(arp_pkt_t)) return;
    p = (arp_pkt_t*)data;

    if (ntohs(p->htype) != 1)           return;
    if (ntohs(p->ptype) != ETH_TYPE_IP) return;

    spa = ntohl(p->spa);
    tpa = ntohl(p->tpa);

    cache_put(spa, p->sha);

    if (ntohs(p->op) == ARP_OP_REQUEST && tpa == my_ip)
        arp_send(ARP_OP_REPLY, p->sha, spa);
}

int arp_resolve(u32 ip, u8 *mac_out) {
    u8  zero[ETH_ALEN] = {0};
    u32 t;
    u8  i;

    if (cache_get(ip, mac_out) == 0) return 0;

    for (i = 0; i < 3; i++) {
        arp_send(ARP_OP_REQUEST, zero, ip);
        t = (u32)pit_get_ticks() + 500;
        while ((u32)pit_get_ticks() < t) {
            ne2000_poll();
            if (cache_get(ip, mac_out) == 0) return 0;
        }
    }
    return -1;
}

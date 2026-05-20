#include "../../include/net/eth.h"
#include "../../include/net/ne2000.h"
#include "../../include/net/arp.h"
#include "../../include/net/ip.h"
#include "../../lib/string.h"
#include "../../lib/types.h"

static u8 mac[ETH_ALEN];

void eth_init(void) {
    ne2000_get_mac(mac);
}

void eth_get_mac(u8 *out) {
    memcpy(out, mac, ETH_ALEN);
}

int eth_send(const u8 *dst, u16 type, const void *data, u16 len) {
    u8 frame[ETH_FRAME_MAX];
    eth_hdr_t *h = (eth_hdr_t*)frame;

    if (len + sizeof(eth_hdr_t) > ETH_FRAME_MAX) return -1;

    memcpy(h->dst, dst, ETH_ALEN);
    memcpy(h->src, mac, ETH_ALEN);
    h->type = htons(type);
    memcpy(frame + sizeof(eth_hdr_t), data, len);

    return ne2000_send(frame, sizeof(eth_hdr_t) + len);
}

void eth_recv(const u8 *frame, u16 len) {
    eth_hdr_t *h;
    u16 type;

    if (len < (u16)sizeof(eth_hdr_t)) return;
    h    = (eth_hdr_t*)frame;
    type = ntohs(h->type);

    switch (type) {
    case ETH_TYPE_ARP:
        arp_recv(frame + sizeof(eth_hdr_t), len - sizeof(eth_hdr_t));
        break;
    case ETH_TYPE_IP:
        ip_recv(frame + sizeof(eth_hdr_t), len - sizeof(eth_hdr_t));
        break;
    default:
        break;
    }
}

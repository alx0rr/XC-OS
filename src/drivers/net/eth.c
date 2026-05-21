#include "../../include/net/eth.h"
#include "../../include/net/ne2000.h"
#include "../../include/net/arp.h"
#include "../../include/net/ip.h"
#include "../../lib/string.h"
#include "../../lib/types.h"
#include "../../include/text.h"

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

    printf("[ETH] send type=0x%04x len=%u dst=%02x:%02x:%02x:%02x:%02x:%02x\n",
        (unsigned)type, (unsigned)len,
        dst[0],dst[1],dst[2],dst[3],dst[4],dst[5]);

    return ne2000_send(frame, sizeof(eth_hdr_t) + len);
}

void eth_recv(const u8 *frame, u16 len) {
    eth_hdr_t *h;
    u16 type;

    printf("[ETH] recv len=%u\n", (unsigned)len);

    if (len < (u16)sizeof(eth_hdr_t)) {
        printf("[ETH] recv too short\n");
        return;
    }
    h    = (eth_hdr_t*)frame;
    type = ntohs(h->type);

    printf("[ETH] recv type=0x%04x src=%02x:%02x:%02x:%02x:%02x:%02x\n",
        (unsigned)type,
        h->src[0],h->src[1],h->src[2],h->src[3],h->src[4],h->src[5]);

    switch (type) {
    case ETH_TYPE_ARP:
        printf("[ETH] -> ARP\n");
        arp_recv(frame + sizeof(eth_hdr_t), len - sizeof(eth_hdr_t));
        break;
    case ETH_TYPE_IP:
        printf("[ETH] -> IP\n");
        ip_recv(frame + sizeof(eth_hdr_t), len - sizeof(eth_hdr_t));
        break;
    default:
        printf("[ETH] -> unknown type 0x%04x, drop\n", (unsigned)type);
        break;
    }
}

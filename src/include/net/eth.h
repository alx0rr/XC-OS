#ifndef ETH_H
#define ETH_H

#include "../../lib/types.h"
#include "ne2000.h"

#define ETH_TYPE_IP  0x0800
#define ETH_TYPE_ARP 0x0806

typedef struct {
    u8  dst[ETH_ALEN];
    u8  src[ETH_ALEN];
    u16 type;
} PACKED eth_hdr_t;

void eth_init(void);
int  eth_send(const u8 *dst, u16 type, const void *data, u16 len);
void eth_recv(const u8 *frame, u16 len);
void eth_get_mac(u8 *out);

static inline u16 htons(u16 v) { return (v >> 8) | (v << 8); }
static inline u16 ntohs(u16 v) { return htons(v); }
static inline u32 htonl(u32 v) {
    return ((v & 0xFF) << 24) | ((v & 0xFF00) << 8) |
           ((v >> 8) & 0xFF00) | ((v >> 24) & 0xFF);
}
static inline u32 ntohl(u32 v) { return htonl(v); }

#endif

#ifndef ARP_H
#define ARP_H

#include "../../lib/types.h"
#include "ne2000.h"

#define ARP_OP_REQUEST 1
#define ARP_OP_REPLY   2

typedef struct {
    u16 htype;
    u16 ptype;
    u8  hlen;
    u8  plen;
    u16 op;
    u8  sha[ETH_ALEN];
    u32 spa;
    u8  tha[ETH_ALEN];
    u32 tpa;
} PACKED arp_pkt_t;

void arp_init(u32 ip);
void arp_recv(const u8 *data, u16 len);
int  arp_resolve(u32 ip, u8 *mac_out);
void arp_set_ip(u32 ip);
u32  arp_get_ip(void);

#endif

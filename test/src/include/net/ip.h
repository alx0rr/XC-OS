#ifndef IP_H
#define IP_H

#include "../../lib/types.h"

typedef struct {
    u8  ihl_ver;
    u8  tos;
    u16 tot_len;
    u16 id;
    u16 frag_off;
    u8  ttl;
    u8  proto;
    u16 chk;
    u32 src;
    u32 dst;
} PACKED ip_hdr_t;

#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP  6
#define IP_PROTO_UDP  17
#define IP_PROTO_UDP  17

void ip_recv(const u8 *data, u16 len);
int  ip_send(u32 dst, u8 proto, const void *data, u16 len);
u16  ip_checksum(const void *data, u16 len);

#endif

#ifndef ICMP_H
#define ICMP_H

#include "../../lib/types.h"

#define ICMP_ECHO_REQUEST 8
#define ICMP_ECHO_REPLY   0

typedef struct {
    u8  type;
    u8  code;
    u16 chk;
    u16 id;
    u16 seq;
} PACKED icmp_hdr_t;

void icmp_recv(u32 src_ip, const u8 *data, u16 len);
int  icmp_ping(u32 ip, u16 seq, u32 *rtt_ms);

#endif

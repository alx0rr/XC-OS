#include "../../include/net/icmp.h"
#include "../../include/net/ip.h"
#include "../../include/net/ne2000.h"
#include "../../include/timer/pit.h"
#include "../../lib/string.h"
#include "../../lib/types.h"
#include "../../include/net/eth.h"

#define PING_ID   0xCA75
#define PING_DATA 32

static volatile u16 ping_recv_seq = 0xFFFF;
static volatile u32 ping_recv_time = 0;

void icmp_recv(u32 src_ip, const u8 *data, u16 len) {
    u8 buf[sizeof(icmp_hdr_t) + 64];
    icmp_hdr_t *ih;
    icmp_hdr_t *rep;

    if (len < (u16)sizeof(icmp_hdr_t)) return;
    ih = (icmp_hdr_t*)data;

    if (ih->type == ICMP_ECHO_REQUEST) {
        u16 plen = len;
        if (plen > sizeof(buf)) plen = sizeof(buf);
        memcpy(buf, data, plen);
        rep = (icmp_hdr_t*)buf;
        rep->type = ICMP_ECHO_REPLY;
        rep->chk  = 0;
        rep->chk  = ip_checksum(buf, plen);
        ip_send(src_ip, IP_PROTO_ICMP, buf, plen);
        return;
    }

    if (ih->type == ICMP_ECHO_REPLY && ntohs(ih->id) == PING_ID) {
        ping_recv_seq  = ntohs(ih->seq);
        ping_recv_time = (u32)pit_get_ticks();
    }
}

int icmp_ping(u32 ip, u16 seq, u32 *rtt_ms) {
    u8 buf[sizeof(icmp_hdr_t) + PING_DATA];
    icmp_hdr_t *h = (icmp_hdr_t*)buf;
    u32 t_start, t_end, deadline;
    u8 i;

    memset(buf, 0, sizeof(buf));
    h->type = ICMP_ECHO_REQUEST;
    h->code = 0;
    h->chk  = 0;
    h->id   = htons(PING_ID);
    h->seq  = htons(seq);
    for (i = 0; i < PING_DATA; i++)
        buf[sizeof(icmp_hdr_t) + i] = (u8)i;
    h->chk = ip_checksum(buf, sizeof(buf));

    ping_recv_seq = 0xFFFF;
    t_start = (u32)pit_get_ticks();

    if (ip_send(ip, IP_PROTO_ICMP, buf, sizeof(buf)) < 0) return -1;

    deadline = t_start + 2000;
    while ((u32)pit_get_ticks() < deadline) {
        ne2000_poll();
        if (ping_recv_seq == seq) {
            t_end = ping_recv_time;
            *rtt_ms = t_end - t_start;
            return 0;
        }
    }
    return -1;
}

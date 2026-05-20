#include "../../include/net/dns.h"
#include "../../include/net/udp.h"
#include "../../include/net/eth.h"
#include "../../include/timer/pit.h"
#include "../../include/net/ne2000.h"
#include "../../lib/string.h"
#include "../../lib/types.h"

#define DNS_PORT      53
#define DNS_SRC_PORT  1053
#define DNS_CACHE_SZ  16
#define DNS_TIMEOUT   3000

typedef struct {
    u16 id, flags, qdcount, ancount, nscount, arcount;
} PACKED dns_hdr_t;

typedef struct {
    char name[64];
    u32  ip;
    u8   valid;
} dns_cache_t;

static u32         dns_server = 0x08080808;
static dns_cache_t cache[DNS_CACHE_SZ];
static volatile u32 recv_ip  = 0;
static volatile u8  recv_ok  = 0;
static u16         txid      = 0x1234;

static void dns_recv_handler(u32 src_ip, u16 src_port, const u8 *data, u16 len);

void dns_init(u32 server_ip) {
    dns_server = server_ip;
    memset(cache, 0, sizeof(cache));
    udp_register(DNS_SRC_PORT, dns_recv_handler);
}

void dns_set_server(u32 ip) { dns_server = ip; }
u32  dns_get_server(void)   { return dns_server; }

static void cache_put(const char *name, u32 ip) {
    u8 i;
    for (i = 0; i < DNS_CACHE_SZ; i++) {
        if (!cache[i].valid || strcmp(cache[i].name, name) == 0) {
            strncpy(cache[i].name, name, 63);
            cache[i].ip    = ip;
            cache[i].valid = 1;
            return;
        }
    }
    cache[0].valid = 0;
    cache_put(name, ip);
}

static int cache_get(const char *name, u32 *ip) {
    u8 i;
    for (i = 0; i < DNS_CACHE_SZ; i++) {
        if (cache[i].valid && strcmp(cache[i].name, name) == 0) {
            *ip = cache[i].ip;
            return 0;
        }
    }
    return -1;
}

static void encode_name(const char *host, u8 *out, u16 *outlen) {
    u16 pos = 0;
    while (*host) {
        const char *dot = host;
        while (*dot && *dot != '.') dot++;
        u8 len = (u8)(dot - host);
        out[pos++] = len;
        memcpy(out + pos, host, len);
        pos += len;
        host = *dot ? dot + 1 : dot;
    }
    out[pos++] = 0;
    *outlen = pos;
}

static void dns_recv_handler(u32 src_ip, u16 src_port, const u8 *data, u16 len) {
    dns_hdr_t *h;
    u16 ancount;
    u16 pos;
    (void)src_ip; (void)src_port;

    if (len < sizeof(dns_hdr_t)) return;
    h = (dns_hdr_t*)data;
    if (ntohs(h->id) != txid) return;
    ancount = ntohs(h->ancount);
    if (!ancount) return;

    pos = sizeof(dns_hdr_t);
    while (pos < len && data[pos]) {
        pos += data[pos] + 1;
    }
    pos += 5;

    u8 attempts = 0;
    while (attempts++ < ancount && pos + 12 <= len) {
        if ((data[pos] & 0xC0) == 0xC0) pos += 2;
        else { while (pos < len && data[pos]) pos += data[pos] + 1; pos++; }

        u16 rtype  = (data[pos] << 8) | data[pos+1]; pos += 2;
        pos += 2;
        pos += 4;
        u16 rdlen  = (data[pos] << 8) | data[pos+1]; pos += 2;

        if (rtype == 1 && rdlen == 4 && pos + 4 <= len) {
            recv_ip = ((u32)data[pos]<<24)|((u32)data[pos+1]<<16)|
                      ((u32)data[pos+2]<<8)|data[pos+3];
            recv_ok = 1;
            return;
        }
        pos += rdlen;
    }
}

int dns_resolve(const char *hostname, u32 *ip_out) {
    u8  pkt[512];
    u8  name_enc[128];
    u16 name_len;
    u32 deadline;
    u16 pos;

    if (cache_get(hostname, ip_out) == 0) return 0;

    encode_name(hostname, name_enc, &name_len);

    txid++;
    dns_hdr_t *h = (dns_hdr_t*)pkt;
    h->id      = htons(txid);
    h->flags   = htons(0x0100);
    h->qdcount = htons(1);
    h->ancount = 0;
    h->nscount = 0;
    h->arcount = 0;

    pos = sizeof(dns_hdr_t);
    memcpy(pkt + pos, name_enc, name_len); pos += name_len;
    pkt[pos++] = 0x00; pkt[pos++] = 0x01;
    pkt[pos++] = 0x00; pkt[pos++] = 0x01;

    recv_ok = 0;
    recv_ip = 0;

    udp_send(dns_server, DNS_SRC_PORT, DNS_PORT, pkt, pos);

    deadline = (u32)pit_get_ticks() + DNS_TIMEOUT;
    while ((u32)pit_get_ticks() < deadline) {
        ne2000_poll();
        if (recv_ok) {
            *ip_out = recv_ip;
            cache_put(hostname, recv_ip);
            return 0;
        }
    }
    return -1;
}

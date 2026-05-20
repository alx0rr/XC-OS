#include "../../include/net/tcp.h"
#include "../../include/net/ip.h"
#include "../../include/net/arp.h"
#include "../../include/net/eth.h"
#include "../../include/net/ne2000.h"
#include "../../include/timer/pit.h"
#include "../../lib/string.h"
#include "../../lib/types.h"
#include "../../include/text.h"

static tcp_conn_t *active_conn = 0;

static u16 tcp_checksum(u32 src, u32 dst, const u8 *seg, u16 seg_len) {
    u32 sum = 0;
    sum += (src >> 16) & 0xFFFF;
    sum += src & 0xFFFF;
    sum += (dst >> 16) & 0xFFFF;
    sum += dst & 0xFFFF;
    sum += htons(IP_PROTO_TCP);
    sum += htons(seg_len);
    const u16 *p = (const u16*)seg;
    u16 len = seg_len;
    while (len > 1) { sum += *p++; len -= 2; }
    if (len) sum += *(u8*)p;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (u16)~sum;
}

static int tcp_send_raw(tcp_conn_t *conn, u8 flags, const void *data, u16 len) {
    u8        buf[sizeof(tcp_hdr_t) + 1500];
    tcp_hdr_t *h = (tcp_hdr_t*)buf;
    u16 total = sizeof(tcp_hdr_t) + len;

    if (total > sizeof(buf)) return -1;

    h->src_port = htons(conn->local_port);
    h->dst_port = htons(conn->remote_port);
    h->seq      = htonl(conn->seq);
    h->ack      = htonl(conn->ack);
    h->data_off = (sizeof(tcp_hdr_t) / 4) << 4;
    h->flags    = flags;
    h->window   = htons(8192);
    h->chk      = 0;
    h->urg      = 0;

    if (len) memcpy(buf + sizeof(tcp_hdr_t), data, len);

    h->chk = tcp_checksum(arp_get_ip(), conn->remote_ip, buf, total);

    return ip_send(conn->remote_ip, IP_PROTO_TCP, buf, total);
}

void tcp_recv_packet(u32 src_ip, const u8 *data, u16 len) {
    tcp_hdr_t *h;
    u8  doff, flags;
    u16 payload_len;

    if (!active_conn) return;
    if (len < (u16)sizeof(tcp_hdr_t)) return;

    h    = (tcp_hdr_t*)data;
    doff = (h->data_off >> 4) * 4;
    if (doff > len) return;

    flags       = h->flags;
    payload_len = len - doff;

    if (active_conn->state == TCP_SYN_SENT) {
        if (ntohs(h->dst_port) != active_conn->local_port) return;
        if ((flags & (TCP_FLAG_SYN | TCP_FLAG_ACK)) == (TCP_FLAG_SYN | TCP_FLAG_ACK)) {
            active_conn->peer_ip = src_ip;
            active_conn->ack = ntohl(h->seq) + 1;
            active_conn->seq++;
            active_conn->state = TCP_ESTABLISHED;
            tcp_send_raw(active_conn, TCP_FLAG_ACK, 0, 0);
        }
        return;
    }

    if (ntohs(h->src_port) != active_conn->remote_port ||
        src_ip              != active_conn->peer_ip) return;

    if (active_conn->state == TCP_ESTABLISHED || active_conn->state == TCP_FIN_WAIT) {
        if (payload_len > 0) {
            u16 space = sizeof(active_conn->rx_buf) - active_conn->rx_len;
            u16 copy  = payload_len < space ? payload_len : space;
            u16 tail  = (active_conn->rx_head + active_conn->rx_len) % sizeof(active_conn->rx_buf);
            u16 part  = sizeof(active_conn->rx_buf) - tail;
            if (copy <= part) {
                memcpy(active_conn->rx_buf + tail, data + doff, copy);
            } else {
                memcpy(active_conn->rx_buf + tail, data + doff, part);
                memcpy(active_conn->rx_buf, data + doff + part, copy - part);
            }
            active_conn->rx_len += copy;
            active_conn->ack    += copy;
            tcp_send_raw(active_conn, TCP_FLAG_ACK, 0, 0);
        }

        if (flags & TCP_FLAG_FIN) {
            active_conn->ack++;
            tcp_send_raw(active_conn, TCP_FLAG_ACK, 0, 0);
            active_conn->state = TCP_TIME_WAIT;
        }
    }
}

int tcp_connect(tcp_conn_t *conn, u32 ip, u16 port) {
    u32 deadline;
    static u16 local_port_ctr = 49152;

    memset(conn, 0, sizeof(*conn));
    conn->remote_ip   = ip;
    conn->remote_port = port;
    conn->local_port  = local_port_ctr++;
    conn->seq         = 0xABCD1234;
    conn->state       = TCP_SYN_SENT;
    active_conn       = conn;

    tcp_send_raw(conn, TCP_FLAG_SYN, 0, 0);

    deadline = (u32)pit_get_ticks() + 5000;
    asm volatile("sti");
    while ((u32)pit_get_ticks() < deadline) {
        asm volatile("hlt");
        ne2000_poll();
        if (conn->state == TCP_ESTABLISHED) return 0;
    }

    conn->state = TCP_CLOSED;
    active_conn = 0;
    return -1;
}

int tcp_send(tcp_conn_t *conn, const void *data, u16 len) {
    if (conn->state != TCP_ESTABLISHED) return -1;
    if (tcp_send_raw(conn, TCP_FLAG_ACK | TCP_FLAG_PSH, data, len) < 0) return -1;
    conn->seq += len;
    return 0;
}

int tcp_recv(tcp_conn_t *conn, void *buf, u16 maxlen, u32 timeout_ms) {
    u32 deadline = (u32)pit_get_ticks() + timeout_ms;
    asm volatile("sti");
    while ((u32)pit_get_ticks() < deadline) {
        asm volatile("hlt");
        ne2000_poll();
        if (conn->rx_len > 0) {
            u16 copy = conn->rx_len < maxlen ? conn->rx_len : maxlen;
            u16 part = sizeof(conn->rx_buf) - conn->rx_head;
            if (copy <= part) {
                memcpy(buf, conn->rx_buf + conn->rx_head, copy);
            } else {
                memcpy(buf, conn->rx_buf + conn->rx_head, part);
                memcpy((u8*)buf + part, conn->rx_buf, copy - part);
            }
            conn->rx_head = (conn->rx_head + copy) % sizeof(conn->rx_buf);
            conn->rx_len -= copy;
            return (int)copy;
        }
        if (conn->state == TCP_TIME_WAIT || conn->state == TCP_CLOSED) break;
    }
    return 0;
}

void tcp_close(tcp_conn_t *conn) {
    if (conn->state == TCP_ESTABLISHED) {
        tcp_send_raw(conn, TCP_FLAG_FIN | TCP_FLAG_ACK, 0, 0);
        conn->seq++;
        conn->state = TCP_FIN_WAIT;
        u32 deadline = (u32)pit_get_ticks() + 2000;
        asm volatile("sti");
        while ((u32)pit_get_ticks() < deadline) {
            asm volatile("hlt");
            ne2000_poll();
            if (conn->state == TCP_TIME_WAIT) break;
        }
    }
    conn->state = TCP_CLOSED;
    if (active_conn == conn) active_conn = 0;
}

#ifndef TCP_H
#define TCP_H

#include "../../lib/types.h"

#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10

typedef struct {
    u16 src_port;
    u16 dst_port;
    u32 seq;
    u32 ack;
    u8  data_off;
    u8  flags;
    u16 window;
    u16 chk;
    u16 urg;
} PACKED tcp_hdr_t;

typedef enum {
    TCP_CLOSED = 0,
    TCP_SYN_SENT,
    TCP_ESTABLISHED,
    TCP_FIN_WAIT,
    TCP_TIME_WAIT,
} tcp_state_t;

typedef struct {
    u32        remote_ip;
    u32        peer_ip;
    u16        remote_port;
    u16        local_port;
    u32        seq;
    u32        ack;
    tcp_state_t state;
    u8         rx_buf[8192];
    u16        rx_len;
    u16        rx_head;
} tcp_conn_t;

int  tcp_connect(tcp_conn_t *conn, u32 ip, u16 port);
int  tcp_send(tcp_conn_t *conn, const void *data, u16 len);
int  tcp_recv(tcp_conn_t *conn, void *buf, u16 maxlen, u32 timeout_ms);
void tcp_close(tcp_conn_t *conn);
void tcp_recv_packet(u32 src_ip, const u8 *data, u16 len);

#endif

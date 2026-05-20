#ifndef UDP_H
#define UDP_H

#include "../../lib/types.h"

typedef struct {
    u16 src_port;
    u16 dst_port;
    u16 len;
    u16 chk;
} PACKED udp_hdr_t;

typedef void (*udp_handler_t)(u32 src_ip, u16 src_port, const u8 *data, u16 len);

void udp_register(u16 port, udp_handler_t handler);
void udp_unregister(u16 port);
int  udp_send(u32 dst_ip, u16 src_port, u16 dst_port, const void *data, u16 len);
void udp_recv(u32 src_ip, const u8 *data, u16 len);

#endif

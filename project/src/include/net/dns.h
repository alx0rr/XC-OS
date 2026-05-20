#ifndef DNS_H
#define DNS_H

#include "../../lib/types.h"

void dns_init(u32 server_ip);
void dns_set_server(u32 server_ip);
u32  dns_get_server(void);
int  dns_resolve(const char *hostname, u32 *ip_out);

#endif

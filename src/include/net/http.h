#ifndef HTTP_H
#define HTTP_H

#include "../../lib/types.h"

typedef struct {
    int  status;
    u32  content_length;
    u8  *body;
    u32  body_len;
} http_response_t;

int http_get(const char *host, u16 port, const char *path,
             u8 *buf, u32 bufsz, u32 *out_len, int *out_status);

int http_post(const char *host, u16 port, const char *path,
              const char *content_type,
              const u8 *body, u32 body_len,
              u8 *buf, u32 bufsz, u32 *out_len, int *out_status);

#endif

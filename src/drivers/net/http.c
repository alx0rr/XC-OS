#include "../../include/net/http.h"
#include "../../include/net/tcp.h"
#include "../../include/net/dns.h"
#include "../../lib/string.h"
#include "../../lib/types.h"

static u32 parse_ip_or_resolve(const char *host) {
    u32 ip = 0;
    const char *p = host;
    u8 dots = 0;
    while (*p) { if (*p == '.') dots++; p++; }
    if (dots == 3) {
        u8 b = 0, i;
        p = host;
        for (i = 0; i < 4; i++) {
            b = 0;
            while (*p >= '0' && *p <= '9') b = b * 10 + (*p++ - '0');
            ip = (ip << 8) | b;
            if (i < 3) p++;
        }
        return ip;
    }
    if (dns_resolve(host, &ip) < 0) return 0;
    return ip;
}

static int find_header_end(const u8 *buf, u32 len) {
    u32 i;
    for (i = 0; i + 3 < len; i++)
        if (buf[i]=='\r' && buf[i+1]=='\n' && buf[i+2]=='\r' && buf[i+3]=='\n')
            return (int)(i + 4);
    return -1;
}

static int parse_status(const u8 *buf, u32 len) {
    u32 i = 0;
    int status = 0;
    while (i < len && buf[i] != ' ') i++;
    i++;
    while (i < len && buf[i] >= '0' && buf[i] <= '9')
        status = status * 10 + (buf[i++] - '0');
    return status;
}

static int ci_match(const u8 *a, const char *b, u16 n) {
    u16 i;
    for (i = 0; i < n; i++) {
        char ac = (a[i] >= 'A' && a[i] <= 'Z') ? a[i] + 32 : a[i];
        char bc = (b[i] >= 'A' && b[i] <= 'Z') ? b[i] + 32 : b[i];
        if (ac != bc) return 0;
    }
    return 1;
}

static u32 parse_content_length(const u8 *buf, u32 hdr_end) {
    u32 i = 0;
    while (i + 16 < hdr_end) {
        if (ci_match(buf + i, "content-length:", 15)) {
            i += 15;
            while (buf[i] == ' ') i++;
            u32 v = 0;
            while (buf[i] >= '0' && buf[i] <= '9') v = v * 10 + (buf[i++] - '0');
            return v;
        }
        while (i < hdr_end && buf[i] != '\n') i++;
        i++;
    }
    return 0;
}

static int do_request(const char *host, u16 port, const char *req, u32 req_len,
                      u8 *buf, u32 bufsz, u32 *out_len, int *out_status) {
    tcp_conn_t conn;
    u32 ip, total = 0;
    int hdr_end = -1;
    u32 content_len = 0;

    ip = parse_ip_or_resolve(host);
    if (!ip) return -1;

    if (tcp_connect(&conn, ip, port) < 0) return -1;
    if (tcp_send(&conn, req, (u16)req_len) < 0) { tcp_close(&conn); return -1; }

    while (total < bufsz - 1) {
        int n = tcp_recv(&conn, buf + total, (u16)(bufsz - 1 - total), 5000);
        if (n <= 0) break;
        total += (u32)n;

        if (hdr_end < 0) {
            hdr_end = find_header_end(buf, total);
            if (hdr_end >= 0) {
                *out_status = parse_status(buf, total);
                content_len = parse_content_length(buf, (u32)hdr_end);
            }
        }
        if (hdr_end >= 0 && content_len > 0) {
            if (total - (u32)hdr_end >= content_len) break;
        }
        if (conn.state == TCP_TIME_WAIT || conn.state == TCP_CLOSED) break;
    }

    tcp_close(&conn);
    buf[total] = 0;
    *out_len = total;
    return 0;
}

int http_get(const char *host, u16 port, const char *path,
             u8 *buf, u32 bufsz, u32 *out_len, int *out_status) {
    char req[512];
    u32  req_len;

    req_len = (u32)snprintf(req, sizeof(req),
        "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n",
        path, host);

    return do_request(host, port, req, req_len, buf, bufsz, out_len, out_status);
}

int http_post(const char *host, u16 port, const char *path,
              const char *content_type,
              const u8 *body, u32 body_len,
              u8 *buf, u32 bufsz, u32 *out_len, int *out_status) {
    char hdr[512];
    u32  hdr_len;
    char full[8192];
    u32  total;

    hdr_len = (u32)snprintf(hdr, sizeof(hdr),
        "POST %s HTTP/1.0\r\nHost: %s\r\nContent-Type: %s\r\nContent-Length: %u\r\nConnection: close\r\n\r\n",
        path, host, content_type, body_len);

    total = hdr_len + body_len;
    if (total > sizeof(full)) return -1;
    memcpy(full, hdr, hdr_len);
    memcpy(full + hdr_len, body, body_len);

    return do_request(host, port, full, total, buf, bufsz, out_len, out_status);
}

#include "../include/editor.h"
#include "../include/fs/xcfs.h"
#include "../include/text.h"
#include "../include/graphics/framebuffer.h"
#include "../include/memory/pmm.h"
#include "../include/input/keyboard.h"
#include "../lib/string.h"
#include "../lib/types.h"

#define ED_BUF_MAX  65536
#define CHAR_W      8
#define CHAR_H      14
#define KEY_UP      17
#define KEY_LEFT    18
#define KEY_RIGHT   19
#define KEY_DOWN    20

extern volatile uint8_t ctrl_pressed;

static char  buf[ED_BUF_MAX];
static u32   buf_len;
static u32   cursor;
static char  filepath[256];
static u8    dirty;

static u32   top_line;

#define MAX_ROWS 64
static char  cache[MAX_ROWS][256];
static u8    cache_valid[MAX_ROWS];

static u32 scr_w, scr_h, content_rows;

#define CMD_MAX 32
static char cmd_buf[CMD_MAX];
static u8   cmd_mode;
static u32  cmd_len;

static u32 ls(u32 pos) {
    while (pos > 0 && buf[pos-1] != '\n') pos--;
    return pos;
}
static u32 le(u32 pos) {
    while (pos < buf_len && buf[pos] != '\n') pos++;
    return pos;
}
static u32 lof(u32 pos) {
    u32 n = 0, i;
    for (i = 0; i < pos; i++) if (buf[i] == '\n') n++;
    return n;
}

static void ins(char c) {
    if (buf_len >= ED_BUF_MAX - 1) return;
    memmove(buf + cursor + 1, buf + cursor, buf_len - cursor);
    buf[cursor++] = c;
    buf_len++;
    dirty = 1;
}
static void delb(void) {
    if (!cursor) return;
    memmove(buf + cursor - 1, buf + cursor, buf_len - cursor);
    buf_len--;
    cursor--;
    dirty = 1;
}
static void delf(void) {
    if (cursor >= buf_len) return;
    memmove(buf + cursor, buf + cursor + 1, buf_len - cursor - 1);
    buf_len--;
    dirty = 1;
}

static void draw_row(u32 row, const char *text, u32 len, u8 is_cr, u32 ccol) {
    u16 y = (u16)(row * CHAR_H);
    fb_fill_rect(0, y, (u16)scr_w, CHAR_H, 0x000000);

    char ln[256];
    u32  dl = len < 255 ? len : 255;
    memcpy(ln, text, dl);
    ln[dl] = '\0';
    if (dl > 0)
        print_at_pos(ln, 0, y, 0xFFFFFF, 0x000000);

    if (is_cr) {
        u16 cx = (u16)(ccol * CHAR_W);
        fb_fill_rect(cx, y, 2, CHAR_H, 0xFFFFFF);
    }

    memcpy(cache[row], ln, dl + 1);
    cache_valid[row] = 1;
}

static void draw_statusbar(void) {
    u16 sy = (u16)(content_rows * CHAR_H);
    fb_fill_rect(0, sy, (u16)scr_w, CHAR_H, 0xAAAAAA);

    if (cmd_mode) {
        char tmp[CMD_MAX + 2];
        tmp[0] = ':';
        memcpy(tmp + 1, cmd_buf, cmd_len);
        tmp[cmd_len + 1] = '\0';
        print_at_pos(tmp, 0, sy, 0x000000, 0xAAAAAA);
    } else {
        char st[128];
        snprintf(st, sizeof(st), " %s%s  :!s Save  :!q Quit",
                 filepath, dirty ? " [+]" : "");
        print_at_pos(st, 0, sy, 0x000000, 0xAAAAAA);
    }
}

static void render_all(void) {
    u32 cl = lof(cursor);
    if (cl < top_line) top_line = cl;
    if (cl >= top_line + content_rows) top_line = cl - content_rows + 1;

    u32 i = 0, line = 0;
    while (i <= buf_len && line < top_line) {
        if (i == buf_len || buf[i] == '\n') line++;
        if (i < buf_len) i++;
    }

    u32 row = 0;
    while (row < content_rows) {
        u32 start = i;
        while (i < buf_len && buf[i] != '\n') i++;
        u32 len   = i - start;
        u32 is_cr = (line == cl);
        u32 col   = is_cr ? (cursor - ls(cursor)) : 0;
        draw_row(row, buf + start, len, (u8)is_cr, col);
        if (i < buf_len) i++;
        line++;
        row++;
    }
    for (; row < content_rows; row++) {
        if (cache_valid[row]) {
            fb_fill_rect(0, (u16)(row * CHAR_H), (u16)scr_w, CHAR_H, 0x000000);
            cache_valid[row] = 0;
        }
    }
    draw_statusbar();
}

static void sav(void) {
    xcfs_dirent_t info;
    if (xcfs_stat(filepath, &info) == 0) xcfs_delete(filepath);
    xcfs_create(filepath, buf_len);
    xcfs_write(filepath, (u8*)buf, buf_len);
    dirty = 0;
    draw_statusbar();
}

static u8 cmd_exec(void) {
    if (cmd_len == 2 && cmd_buf[0] == '!' && (cmd_buf[1] == 's' || cmd_buf[1] == 'S')) {
        sav();
        return 0;
    }
    if (cmd_len == 2 && cmd_buf[0] == '!' && (cmd_buf[1] == 'q' || cmd_buf[1] == 'Q')) {
        return 1;
    }
    return 0;
}

void editor_open(const char *path) {
    strncpy(filepath, path, 255);
    buf_len  = 0;
    cursor   = 0;
    dirty    = 0;
    top_line = 0;
    cmd_mode = 0;
    cmd_len  = 0;
    memset(buf, 0, sizeof(buf));
    memset(cache_valid, 0, sizeof(cache_valid));

    scr_w        = fb_get_width();
    scr_h        = fb_get_height();
    content_rows = scr_h / CHAR_H - 1;
    if (content_rows > MAX_ROWS) content_rows = MAX_ROWS;

    xcfs_dirent_t info;
    if (xcfs_stat(path, &info) == 0 && info.size > 0) {
        u32 sz = info.size < ED_BUF_MAX - 1 ? info.size : ED_BUF_MAX - 1;
        xcfs_read(path, (u8*)buf, sz);
        buf_len = sz;
    }

    fb_fill(0x000000);
    render_all();

    while (1) {
        char c = keyboard_getchar_raw();

        if (cmd_mode) {
            if (c == '\n' || c == '\r') {
                cmd_mode = 0;
                if (cmd_exec()) break;
                cmd_len = 0;
                render_all();
            } else if (c == '\b') {
                if (cmd_len > 0) cmd_len--;
                draw_statusbar();
            } else if (c == 27) {
                cmd_mode = 0;
                cmd_len  = 0;
                render_all();
            } else if (cmd_len < CMD_MAX - 1 && c >= 32) {
                cmd_buf[cmd_len++] = c;
                cmd_buf[cmd_len]   = '\0';
                draw_statusbar();
            }
            continue;
        }

        if (c == ':') {
            cmd_mode = 1;
            cmd_len  = 0;
            memset(cmd_buf, 0, sizeof(cmd_buf));
            draw_statusbar();
            continue;
        }

        if ((u8)c == KEY_UP) {
            u32 s  = ls(cursor);
            if (s == 0) { cursor = 0; render_all(); continue; }
            u32 col = cursor - s;
            u32 pe  = s - 1;
            u32 ps  = ls(pe);
            u32 pl  = pe - ps;
            cursor  = ps + (col < pl ? col : pl);
            render_all();
            continue;
        }
        if ((u8)c == KEY_DOWN) {
            u32 end = le(cursor);
            if (end >= buf_len) { cursor = buf_len; render_all(); continue; }
            u32 col = cursor - ls(cursor);
            u32 ns  = end + 1;
            u32 ne  = le(ns);
            u32 nl  = ne - ns;
            cursor  = ns + (col < nl ? col : nl);
            render_all();
            continue;
        }
        if ((u8)c == KEY_LEFT)  { if (cursor > 0) cursor--; render_all(); continue; }
        if ((u8)c == KEY_RIGHT) { if (cursor < buf_len) cursor++; render_all(); continue; }

        if (c == '\b') { delb(); render_all(); continue; }
        if (c == 127)  { delf(); render_all(); continue; }

        if (c >= 32 || c == '\n' || c == '\t') {
            ins(c);
            render_all();
        }
    }

    fb_fill(0x000000);
    xpos = 0; ypos = 0;
}
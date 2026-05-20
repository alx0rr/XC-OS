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

static u32 line_start(u32 pos) {
    while (pos > 0 && buf[pos-1] != '\n') pos--;
    return pos;
}
static u32 line_end(u32 pos) {
    while (pos < buf_len && buf[pos] != '\n') pos++;
    return pos;
}
static u32 line_of(u32 pos) {
    u32 n = 0, i;
    for (i = 0; i < pos; i++) if (buf[i] == '\n') n++;
    return n;
}

static void insert_char(char c) {
    if (buf_len >= ED_BUF_MAX - 1) return;
    memmove(buf + cursor + 1, buf + cursor, buf_len - cursor);
    buf[cursor++] = c;
    buf_len++;
    dirty = 1;
}
static void delete_back(void) {
    if (!cursor) return;
    memmove(buf + cursor - 1, buf + cursor, buf_len - cursor);
    buf_len--;
    cursor--;
    dirty = 1;
}
static void delete_fwd(void) {
    if (cursor >= buf_len) return;
    memmove(buf + cursor, buf + cursor + 1, buf_len - cursor - 1);
    buf_len--;
    dirty = 1;
}

static void draw_row(u32 row, const char *text, u32 len, u8 is_cursor_row, u32 cursor_col) {
    u16 y = (u16)(row * CHAR_H);
    fb_fill_rect(0, y, (u16)scr_w, CHAR_H, 0x000000);

    char line[256];
    u32  draw_len = len < 255 ? len : 255;
    memcpy(line, text, draw_len);
    line[draw_len] = '\0';
    if (draw_len > 0)
        print_at_pos(line, 0, y, 0xFFFFFF, 0x000000);

    if (is_cursor_row) {
        u16 cx = (u16)(cursor_col * CHAR_W);
        fb_fill_rect(cx, y, 2, CHAR_H, 0xFFFFFF);
    }

    memcpy(cache[row], line, draw_len + 1);
    cache_valid[row] = 1;
}

static void draw_statusbar(void) {
    u16 sy = (u16)((content_rows) * CHAR_H);
    fb_fill_rect(0, sy, (u16)scr_w, CHAR_H, 0xAAAAAA);
    char status[128];
    snprintf(status, sizeof(status), " %s%s  ^S Save  ^Q Quit",
             filepath, dirty ? " [+]" : "");
    print_at_pos(status, 0, sy, 0x000000, 0xAAAAAA);
}

static void render_all(void) {
    u32 cur_line = line_of(cursor);
    if (cur_line < top_line) top_line = cur_line;
    if (cur_line >= top_line + content_rows) top_line = cur_line - content_rows + 1;

    u32 i = 0, line = 0;
    while (i <= buf_len && line < top_line) {
        if (i == buf_len || buf[i] == '\n') line++;
        if (i < buf_len) i++;
    }

    u32 row = 0;
    while (row < content_rows) {
        u32 start = i;
        while (i < buf_len && buf[i] != '\n') i++;
        u32 len = i - start;
        u32 is_cur = (line == cur_line);
        u32 col = is_cur ? (cursor - line_start(cursor)) : 0;
        draw_row(row, buf + start, len, (u8)is_cur, col);
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

static void save(void) {
    xcfs_dirent_t info;
    if (xcfs_stat(filepath, &info) == 0) xcfs_delete(filepath);
    xcfs_create(filepath, buf_len);
    xcfs_write(filepath, (u8*)buf, buf_len);
    dirty = 0;
    draw_statusbar();
}

void editor_open(const char *path) {
    strncpy(filepath, path, 255);
    buf_len  = 0;
    cursor   = 0;
    dirty    = 0;
    top_line = 0;
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

        if (ctrl_pressed) {
            if (c == 's' || c == 'S') { save(); continue; }
            if (c == 'q' || c == 'Q') break;
        }

        if ((u8)c == KEY_UP) {
            u32 ls  = line_start(cursor);
            if (ls == 0) { cursor = 0; render_all(); continue; }
            u32 col       = cursor - ls;
            u32 prev_end  = ls - 1;
            u32 prev_start = line_start(prev_end);
            u32 prev_len  = prev_end - prev_start;
            cursor = prev_start + (col < prev_len ? col : prev_len);
            render_all();
            continue;
        }
        if ((u8)c == KEY_DOWN) {
            u32 le = line_end(cursor);
            if (le >= buf_len) { cursor = buf_len; render_all(); continue; }
            u32 col        = cursor - line_start(cursor);
            u32 next_start = le + 1;
            u32 next_end   = line_end(next_start);
            u32 next_len   = next_end - next_start;
            cursor = next_start + (col < next_len ? col : next_len);
            render_all();
            continue;
        }
        if ((u8)c == KEY_LEFT)  { if (cursor > 0) cursor--; render_all(); continue; }
        if ((u8)c == KEY_RIGHT) { if (cursor < buf_len) cursor++; render_all(); continue; }

        if (c == '\b') { delete_back(); render_all(); continue; }
        if (c == 127)  { delete_fwd();  render_all(); continue; }

        if (c >= 32 || c == '\n' || c == '\t') {
            insert_char(c);
            render_all();
        }
    }

    fb_fill(0x000000);
    xpos = 0; ypos = 0;
}

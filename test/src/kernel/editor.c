#include "../include/editor.h"
#include "../include/fs/xcfs.h"
#include "../include/text.h"
#include "../include/memory/pmm.h"
#include "../lib/string.h"
#include "../lib/types.h"
#include "../include/input/keyboard.h"

#define ED_ROWS     128
#define ED_COLS     256
#define ED_BUF_MAX  (ED_ROWS * ED_COLS)

#define KEY_UP    17
#define KEY_LEFT  18
#define KEY_RIGHT 19
#define KEY_DOWN  20

extern volatile uint8_t ctrl_pressed;

static char   buf[ED_BUF_MAX];
static u32    buf_len;
static u32    cursor;
static char   filepath[256];
static u8     dirty;

static u32 line_start(u32 pos) {
    while (pos > 0 && buf[pos - 1] != '\n') pos--;
    return pos;
}

static u32 line_end(u32 pos) {
    while (pos < buf_len && buf[pos] != '\n') pos++;
    return pos;
}

static u32 col_of(u32 pos) {
    return pos - line_start(pos);
}

static void insert_char(char c) {
    if (buf_len >= ED_BUF_MAX - 1) return;
    memmove(buf + cursor + 1, buf + cursor, buf_len - cursor);
    buf[cursor] = c;
    buf_len++;
    cursor++;
    dirty = 1;
}

static void delete_back(void) {
    if (cursor == 0) return;
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

static void render(void) {
    clear();
    u32 scr_w = fb_get_width()  / 8;
    u32 scr_h = fb_get_height() / 14;
    u32 content_rows = scr_h - 2;

    u32 cur_line = 0;
    {
        u32 i;
        for (i = 0; i < cursor; i++)
            if (buf[i] == '\n') cur_line++;
    }

    u32 top_line = 0;
    if (cur_line >= content_rows) top_line = cur_line - content_rows + 1;

    u32 line = 0, i = 0;
    while (i <= buf_len && line < top_line) {
        if (i == buf_len || buf[i] == '\n') line++;
        i++;
    }

    u32 row = 0;
    u32 col = 0;
    u32 cur_row = 0, cur_col_px = 0;

    xpos = 0;
    ypos = 0;
    fg_color = 0xFFFFFF;

    while (i <= buf_len && row < content_rows) {
        if (i == cursor) {
            cur_row     = row;
            cur_col_px  = col * 8;
        }
        if (i == buf_len) break;
        char c = buf[i];
        if (c == '\n') {
            row++;
            col = 0;
            printf("\n");
        } else {
            printf("%c", c);
            col++;
        }
        i++;
    }
    if (i == cursor) {
        cur_row    = row;
        cur_col_px = col * 8;
    }

    u32 status_y = (scr_h - 1) * 14;
    xpos = 0; ypos = (u16)status_y;
    fg_color = 0x000000;
    bg_color = 0xAAAAAA;
    u32 sw = fb_get_width();
    u32 sx;
    for (sx = 0; sx < sw; sx++) fb_putpixel((u16)sx, (u16)status_y, 0xAAAAAA);
    printf(" %s%s  Ctrl+S Save  Ctrl+Q Quit",
           filepath, dirty ? " [modified]" : "");
    bg_color = 0x000000;
    fg_color = 0xFFFFFF;

    u32 cx = (u32)cur_col_px;
    u32 cy = cur_row * 14;
    u32 r;
    for (r = 0; r < 14; r++)
        fb_putpixel((u16)cx, (u16)(cy + r), 0xFFFFFF);
}

static void save(void) {
    xcfs_dirent_t info;
    if (xcfs_stat(filepath, &info) == 0)
        xcfs_delete(filepath);
    xcfs_create(filepath, buf_len);
    xcfs_write(filepath, (u8*)buf, buf_len);
    dirty = 0;
}

void editor_open(const char *path) {
    strncpy(filepath, path, 255);
    buf_len = 0;
    cursor  = 0;
    dirty   = 0;
    memset(buf, 0, sizeof(buf));

    xcfs_dirent_t info;
    if (xcfs_stat(path, &info) == 0 && info.size > 0) {
        u32 sz = info.size < ED_BUF_MAX - 1 ? info.size : ED_BUF_MAX - 1;
        xcfs_read(path, (u8*)buf, sz);
        buf_len = sz;
    }

    while (1) {
        render();

        char c = keyboard_getchar_raw();

        if (ctrl_pressed) {
            if (c == 's' || c == 'S') { save(); continue; }
            if (c == 'q' || c == 'Q') break;
        }

        if ((u8)c == KEY_UP) {
            u32 ls  = line_start(cursor);
            if (ls == 0) { cursor = 0; continue; }
            u32 col = cursor - ls;
            u32 prev_end   = ls - 1;
            u32 prev_start = line_start(prev_end);
            u32 prev_len   = prev_end - prev_start;
            cursor = prev_start + (col < prev_len ? col : prev_len);
            continue;
        }
        if ((u8)c == KEY_DOWN) {
            u32 le  = line_end(cursor);
            if (le >= buf_len) { cursor = buf_len; continue; }
            u32 col       = cursor - line_start(cursor);
            u32 next_start = le + 1;
            u32 next_end   = line_end(next_start);
            u32 next_len   = next_end - next_start;
            cursor = next_start + (col < next_len ? col : next_len);
            continue;
        }
        if ((u8)c == KEY_LEFT)  { if (cursor > 0) cursor--; continue; }
        if ((u8)c == KEY_RIGHT) { if (cursor < buf_len) cursor++; continue; }

        if (c == '\b') { delete_back(); continue; }
        if (c == 127)  { delete_fwd();  continue; }

        if (c >= 32 || c == '\n' || c == '\t')
            insert_char(c);
    }

    clear();
}

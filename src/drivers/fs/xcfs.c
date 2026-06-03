#include "../../include/fs/xcfs.h"
#include "../../include/storage/ata.h"
#include "../../include/memory/pmm.h"
#include "../../include/text.h"
#include "../../lib/string.h"

#define INVAL  ((uint32_t)-1)
#define EPSec  (XCFS_SECTOR_SIZE / sizeof(xcfs_entry_t))

static xcfs_context_t  g_ctx = {0};
static xcfs_header_t   g_hdr = {0};
static xcfs_entry_t*   g_ent = 0;

static void     np(const char* p, char* o);
static int      fe(const char* p);
static int      fp(const char* p);
static void     sv_hdr(void);
static void     sv_ent_range(uint32_t from, uint32_t to);
static void     sv_bmp(void);
static void     bmp_set(uint32_t s, int v);
static int      bmp_tst(uint32_t s);
static uint32_t bmp_find(uint32_t n);
static void     bmp_mark(uint32_t s, uint32_t n, int v);

static void np(const char* p, char* o) {
    if (!p || !o) return;
    char tmp[XCFS_MAX_PATH];
    int i = 0;
    if (p[0] != '/') {
        int cl = strlen(g_ctx.cwd);
        memcpy(tmp, g_ctx.cwd, cl);
        if (cl == 0 || tmp[cl-1] != '/') tmp[cl++] = '/';
        int pl = strlen(p);
        if (cl + pl >= XCFS_MAX_PATH) return;
        memcpy(tmp + cl, p, pl + 1);
    } else {
        int pl = strlen(p);
        if (pl >= XCFS_MAX_PATH) return;
        memcpy(tmp, p, pl + 1);
    }
    char buf[XCFS_MAX_PATH];
    int bi = 0;
    int ti = 0;
    buf[bi++] = '/';
    ti = 1;
    while (tmp[ti]) {
        if (tmp[ti] == '/') { ti++; continue; }
        if (tmp[ti] == '.' && (tmp[ti+1] == '/' || tmp[ti+1] == '\0')) {
            ti += (tmp[ti+1] == '/') ? 2 : 1;
            continue;
        }
        if (tmp[ti] == '.' && tmp[ti+1] == '.' && (tmp[ti+2] == '/' || tmp[ti+2] == '\0')) {
            ti += (tmp[ti+2] == '/') ? 3 : 2;
            if (bi > 1) {
                bi--;
                while (bi > 1 && buf[bi-1] != '/') bi--;
            }
            continue;
        }
        if (bi > 1) buf[bi++] = '/';
        while (tmp[ti] && tmp[ti] != '/') {
            if (bi >= XCFS_MAX_PATH - 1) return;
            buf[bi++] = tmp[ti++];
        }
    }
    buf[bi] = '\0';
    memcpy(o, buf, bi + 1);
    i = bi;
    if (i > 1 && o[i-1] == '/') o[i-1] = '\0';
}

void xcfs_normalize_path(const char* p, char* o) { np(p, o); }

static int fe(const char* p) {
    for (uint32_t i = 0; i < g_hdr.file_count; i++)
        if (strcmp(g_ent[i].path, p) == 0) return (int)i;
    return -1;
}

static int fp(const char* p) {
    char par[XCFS_MAX_PATH];
    int pl = strlen(p);
    if (pl == 0) return -1;
    if (pl == 1 && p[0] == '/') return 0;
    int last = pl - 1;
    while (last > 0 && p[last] != '/') last--;
    if (last == 0) {
        strcpy(par, "/");
    } else {
        memcpy(par, p, last);
        par[last] = '\0';
    }
    return fe(par);
}

static void bmp_set(uint32_t s, int v) {
    if (s < g_ctx.data_start) return;
    uint32_t idx = s - g_ctx.data_start;
    uint32_t by  = idx >> 3;
    uint32_t bi  = idx & 7;
    if (by >= (uint32_t)(g_ctx.bitmap_sectors * XCFS_SECTOR_SIZE)) return;
    if (v) g_ctx.bitmap[by] |=  (uint8_t)(1 << bi);
    else   g_ctx.bitmap[by] &= (uint8_t)~(1 << bi);
}

static int bmp_tst(uint32_t s) {
    if (s < g_ctx.data_start) return 1;
    uint32_t idx = s - g_ctx.data_start;
    uint32_t by  = idx >> 3;
    uint32_t bi  = idx & 7;
    if (by >= (uint32_t)(g_ctx.bitmap_sectors * XCFS_SECTOR_SIZE)) return 1;
    return (g_ctx.bitmap[by] >> bi) & 1;
}

static void bmp_mark(uint32_t s, uint32_t n, int v) {
    for (uint32_t i = 0; i < n; i++) bmp_set(s + i, v);
}

static uint32_t bmp_find(uint32_t n) {
    uint32_t total = g_hdr.total_sectors - g_ctx.data_start;
    uint32_t run = 0, start = 0;
    for (uint32_t i = 0; i < total; i++) {
        uint32_t by = i >> 3;
        uint32_t bi = i & 7;
        if (!(g_ctx.bitmap[by] & (1 << bi))) {
            if (run == 0) start = i;
            if (++run >= n) return g_ctx.data_start + start;
        } else {
            run = 0;
        }
    }
    return INVAL;
}

static void sv_hdr(void) {
    uint8_t buf[XCFS_SECTOR_SIZE] = {0};
    memcpy(buf, &g_hdr, sizeof(xcfs_header_t));
    ata_write_sector(g_ctx.drive, XCFS_START_SECTOR, buf);
}

static void sv_ent_range(uint32_t from, uint32_t to) {
    uint8_t buf[XCFS_SECTOR_SIZE];
    uint32_t fs = from / EPSec;
    uint32_t ts = (to + EPSec - 1) / EPSec;
    for (uint32_t s = fs; s < ts; s++) {
        memset(buf, 0, XCFS_SECTOR_SIZE);
        memcpy(buf, &g_ent[s * EPSec], XCFS_SECTOR_SIZE);
        ata_write_sector(g_ctx.drive, XCFS_START_SECTOR + 1 + s, buf);
    }
}

static void sv_bmp(void) {
    uint32_t bstart = XCFS_START_SECTOR + 1 + g_ctx.max_files;
    for (uint32_t i = 0; i < g_ctx.bitmap_sectors; i++)
        ata_write_sector(g_ctx.drive, bstart + i, &g_ctx.bitmap[i * XCFS_SECTOR_SIZE]);
}

void xcfs_init(uint8_t drv) {
    g_ctx.drive = drv;
    strcpy(g_ctx.cwd, "/");
    uint8_t buf[XCFS_SECTOR_SIZE];
    if (ata_read_sector(drv, XCFS_START_SECTOR, buf) < 0) {
        printf("{FG(255,0,0)}XCFS: disk read fail\n");
        return;
    }
    memcpy(&g_hdr, buf, sizeof(xcfs_header_t));
    if (g_hdr.magic != XCFS_MAGIC || g_hdr.version != XCFS_VERSION) {
        printf("{FG(255,0,0)}[!]{FG(255,255,255)} XCFS v%d not found, formatting...\n", XCFS_VERSION);
        xcfs_format(drv, 131072);
        return;
    }

    g_ctx.max_files      = CFG_XCFS_META_SECTORS;
    g_ctx.bitmap_sectors = (g_hdr.total_sectors + 8 * XCFS_SECTOR_SIZE - 1)
                           / (8 * XCFS_SECTOR_SIZE);
    g_ctx.data_start     = XCFS_DATA_START_SECTOR;

    if (!g_ent) {
        g_ent = (xcfs_entry_t*)pmm_malloc(sizeof(xcfs_entry_t) * g_ctx.max_files);
        if (!g_ent) {
            printf("{FG(255,0,0)}XCFS: out of memory\n");
            return;
        }
    }
    memset(g_ent, 0, sizeof(xcfs_entry_t) * g_ctx.max_files);

    uint32_t fsec = (g_hdr.file_count + EPSec - 1) / EPSec;
    if (fsec > g_ctx.max_files) fsec = g_ctx.max_files;
    for (uint32_t i = 0; i < fsec; i++) {
        if (ata_read_sector(drv, XCFS_START_SECTOR + 1 + i, buf) < 0) return;
        memcpy(&g_ent[i * EPSec], buf, XCFS_SECTOR_SIZE);
    }

    uint32_t bstart = XCFS_START_SECTOR + 1 + g_ctx.max_files;
    for (uint32_t i = 0; i < g_ctx.bitmap_sectors; i++)
        ata_read_sector(drv, bstart + i, &g_ctx.bitmap[i * XCFS_SECTOR_SIZE]);

    g_ctx.initialized = 1;
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} XCFS v%d (%u files, data@%u)\n",
           XCFS_VERSION, g_hdr.file_count, g_ctx.data_start);
}

void xcfs_format(uint8_t drv, uint32_t total) {
    memset(&g_hdr, 0, sizeof(g_hdr));
    g_hdr.magic         = XCFS_MAGIC;
    g_hdr.version       = XCFS_VERSION;
    g_hdr.total_sectors = total;
    g_hdr.file_count    = 0;

    g_ctx.max_files      = CFG_XCFS_META_SECTORS;
    g_ctx.bitmap_sectors = (total + 8 * XCFS_SECTOR_SIZE - 1) / (8 * XCFS_SECTOR_SIZE);
    g_ctx.data_start     = XCFS_DATA_START_SECTOR;

    if (!g_ent) {
        g_ent = (xcfs_entry_t*)pmm_malloc(sizeof(xcfs_entry_t) * g_ctx.max_files);
        if (!g_ent) {
            printf("{FG(255,0,0)}XCFS: out of memory\n");
            return;
        }
    }
    memset(g_ent, 0, sizeof(xcfs_entry_t) * g_ctx.max_files);
    memset(g_ctx.bitmap, 0, sizeof(g_ctx.bitmap));

    xcfs_entry_t* root = &g_ent[0];
    strcpy(root->path, "/");
    root->type         = XCFS_TYPE_DIR;
    root->flags        = XCFS_FLAG_PROTECTED;
    root->parent_idx   = 0;
    root->start_sector = 0;
    root->size         = 0;
    g_hdr.file_count   = 1;

    g_ctx.drive       = drv;
    g_ctx.initialized = 1;
    strcpy(g_ctx.cwd, "/");

    sv_hdr();
    sv_ent_range(0, 1);
    sv_bmp();
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} XCFS v%d formatted (data@%u)\n",
           XCFS_VERSION, g_ctx.data_start);
}

int xcfs_mkdir(const char* path) {
    if (!g_ctx.initialized) return -1;
    if (g_hdr.file_count >= g_ctx.max_files) return -1;
    char norm[XCFS_MAX_PATH];
    np(path, norm);
    if (fe(norm) >= 0) return -1;
    int par = fp(norm);
    if (par < 0) return -1;
    uint32_t idx = g_hdr.file_count;
    xcfs_entry_t* e = &g_ent[idx];
    memset(e, 0, sizeof(*e));
    strncpy(e->path, norm, XCFS_MAX_PATH - 1);
    e->type       = XCFS_TYPE_DIR;
    e->parent_idx = (uint32_t)par;
    g_hdr.file_count++;
    sv_hdr();
    sv_ent_range(idx, idx + 1);
    return 0;
}

int xcfs_create(const char* path, uint32_t size) {
    if (!g_ctx.initialized) return -1;
    if (g_hdr.file_count >= g_ctx.max_files) return -1;
    char norm[XCFS_MAX_PATH];
    np(path, norm);
    if (fe(norm) >= 0) return -1;
    int par = fp(norm);
    if (par < 0) return -1;

    uint32_t nsec = (size + XCFS_SECTOR_SIZE - 1) / XCFS_SECTOR_SIZE;
    if (nsec == 0) nsec = 1;
    uint32_t start = bmp_find(nsec);
    if (start == INVAL) return -1;

    uint32_t idx = g_hdr.file_count;
    xcfs_entry_t* e = &g_ent[idx];
    memset(e, 0, sizeof(*e));
    strncpy(e->path, norm, XCFS_MAX_PATH - 1);
    e->type         = XCFS_TYPE_FILE;
    e->parent_idx   = (uint32_t)par;
    e->start_sector = start;
    e->size         = size;

    bmp_mark(start, nsec, 1);
    g_hdr.file_count++;
    sv_hdr();
    sv_ent_range(idx, idx + 1);
    sv_bmp();

    uint8_t z[XCFS_SECTOR_SIZE] = {0};
    for (uint32_t s = 0; s < nsec; s++)
        ata_write_sector(g_ctx.drive, start + s, z);
    return 0;
}

int xcfs_delete(const char* path) {
    if (!g_ctx.initialized) return -1;
    char norm[XCFS_MAX_PATH];
    np(path, norm);
    if (strcmp(norm, "/") == 0) return -1;

    int idx = fe(norm);
    if (idx < 0) return -1;

    xcfs_entry_t* e = &g_ent[idx];
    if (e->flags & XCFS_FLAG_PROTECTED) return -1;

    uint8_t del_type = e->type;

    if (del_type == XCFS_TYPE_DIR) {
        for (uint32_t i = 0; i < g_hdr.file_count; i++)
            if (i != (uint32_t)idx && g_ent[i].parent_idx == (uint32_t)idx)
                return -1;
    } else {
        uint32_t del_start = e->start_sector;
        uint32_t del_nsec  = (e->size + XCFS_SECTOR_SIZE - 1) / XCFS_SECTOR_SIZE;
        if (del_nsec == 0) del_nsec = 1;

        bmp_mark(del_start, del_nsec, 0);

        for (uint32_t fi = 0; fi < g_hdr.file_count; fi++) {
            if (g_ent[fi].type != XCFS_TYPE_FILE) continue;
            if (fi == (uint32_t)idx) continue;
            if (g_ent[fi].start_sector <= del_start) continue;

            uint32_t file_nsec = (g_ent[fi].size + XCFS_SECTOR_SIZE - 1) / XCFS_SECTOR_SIZE;
            if (file_nsec == 0) file_nsec = 1;

            uint32_t old_start = g_ent[fi].start_sector;
            uint32_t new_start = old_start - del_nsec;

            uint8_t buf[XCFS_SECTOR_SIZE];
            for (uint32_t s = 0; s < file_nsec; s++) {
                ata_read_sector(g_ctx.drive, old_start + s, buf);
                ata_write_sector(g_ctx.drive, new_start + s, buf);
            }

            bmp_mark(old_start, file_nsec, 0);
            bmp_mark(new_start, file_nsec, 1);

            g_ent[fi].start_sector = new_start;
        }
    }

    uint32_t uidx = (uint32_t)idx;
    uint32_t last = g_hdr.file_count - 1;

    if (uidx != last) {
        g_ent[uidx] = g_ent[last];
        for (uint32_t i = 0; i < last; i++)
            if (g_ent[i].parent_idx == last)
                g_ent[i].parent_idx = uidx;
    }
    memset(&g_ent[last], 0, sizeof(xcfs_entry_t));
    g_hdr.file_count--;

    sv_hdr();
    uint32_t lo = uidx < last ? uidx : last;
    sv_ent_range(lo, last + 1);
    if (del_type == XCFS_TYPE_FILE) sv_bmp();
    return 0;
}

int xcfs_read(const char* path, uint8_t* buf, uint32_t size) {
    if (!g_ctx.initialized) return -1;
    char norm[XCFS_MAX_PATH];
    np(path, norm);
    int idx = fe(norm);
    if (idx < 0) return -1;
    xcfs_entry_t* e = &g_ent[idx];
    if (e->type != XCFS_TYPE_FILE) return -1;
    uint32_t rd = size < e->size ? size : e->size;
    uint32_t nsec = (rd + XCFS_SECTOR_SIZE - 1) / XCFS_SECTOR_SIZE;
    uint8_t sb[XCFS_SECTOR_SIZE];
    uint32_t done = 0;
    for (uint32_t s = 0; s < nsec; s++) {
        if (ata_read_sector(g_ctx.drive, e->start_sector + s, sb) < 0) return -1;
        uint32_t cp = rd - done;
        if (cp > XCFS_SECTOR_SIZE) cp = XCFS_SECTOR_SIZE;
        memcpy(buf + done, sb, cp);
        done += cp;
    }
    return (int)done;
}

int xcfs_write(const char* path, uint8_t* buf, uint32_t size) {
    if (!g_ctx.initialized) return -1;
    char norm[XCFS_MAX_PATH];
    np(path, norm);
    int idx = fe(norm);
    if (idx < 0) return -1;
    xcfs_entry_t* e = &g_ent[idx];
    if (e->type != XCFS_TYPE_FILE) return -1;
    if (e->flags & XCFS_FLAG_READONLY) return -1;
    if (size > e->size) return -1;
    uint32_t nsec = (size + XCFS_SECTOR_SIZE - 1) / XCFS_SECTOR_SIZE;
    if (nsec == 0) nsec = 1;
    uint8_t sb[XCFS_SECTOR_SIZE];
    uint32_t done = 0;
    for (uint32_t s = 0; s < nsec; s++) {
        memset(sb, 0, XCFS_SECTOR_SIZE);
        uint32_t cp = size - done;
        if (cp > XCFS_SECTOR_SIZE) cp = XCFS_SECTOR_SIZE;
        memcpy(sb, buf + done, cp);
        if (ata_write_sector(g_ctx.drive, e->start_sector + s, sb) < 0) return -1;
        done += cp;
    }
    e->modified = 0;
    sv_ent_range((uint32_t)idx, (uint32_t)idx + 1);
    return (int)done;
}

int xcfs_readdir(const char* path, xcfs_dirent_t* out, uint32_t max) {
    if (!g_ctx.initialized || !out || max == 0) return -1;
    char norm[XCFS_MAX_PATH];
    np(path, norm);
    int didx = fe(norm);
    if (didx < 0 || g_ent[didx].type != XCFS_TYPE_DIR) return -1;
    uint32_t cnt = 0;
    uint32_t ud  = (uint32_t)didx;
    for (uint32_t i = 0; i < g_hdr.file_count && cnt < max; i++) {
        if (i == ud) continue;
        if (g_ent[i].parent_idx != ud) continue;
        const char* nm = strrchr(g_ent[i].path, '/');
        nm = nm ? nm + 1 : g_ent[i].path;
        strncpy(out[cnt].name, nm, 255);
        out[cnt].name[255] = '\0';
        out[cnt].type  = g_ent[i].type;
        out[cnt].flags = g_ent[i].flags;
        out[cnt].size  = g_ent[i].size;
        cnt++;
    }
    return (int)cnt;
}

int xcfs_stat(const char* path, xcfs_dirent_t* info) {
    if (!g_ctx.initialized || !info) return -1;
    char norm[XCFS_MAX_PATH];
    np(path, norm);
    int idx = fe(norm);
    if (idx < 0) return -1;
    const char* nm = strrchr(norm, '/');
    nm = (nm && *(nm+1)) ? nm + 1 : norm;
    strncpy(info->name, nm, 255);
    info->name[255] = '\0';
    info->type  = g_ent[idx].type;
    info->flags = g_ent[idx].flags;
    info->size  = g_ent[idx].size;
    return 0;
}

int xcfs_chdir(const char* path) {
    if (!g_ctx.initialized) return -1;
    char norm[XCFS_MAX_PATH];
    np(path, norm);
    int idx = fe(norm);
    if (idx < 0 || g_ent[idx].type != XCFS_TYPE_DIR) return -1;
    strncpy(g_ctx.cwd, norm, XCFS_MAX_PATH - 1);
    return 0;
}

const char* xcfs_getcwd(void) { return g_ctx.cwd; }

int xcfs_list(const char* path) {
    if (!g_ctx.initialized) {
        printf("{FG(255,0,0)}XCFS not initialized\n");
        return -1;
    }
    char norm[XCFS_MAX_PATH];
    np(path ? path : g_ctx.cwd, norm);
    xcfs_dirent_t* ents = (xcfs_dirent_t*)pmm_malloc(sizeof(xcfs_dirent_t) * g_ctx.max_files);
    if (!ents) {
        printf("{FG(255,0,0)}ls: out of memory\n");
        return -1;
    }
    int cnt = xcfs_readdir(norm, ents, g_ctx.max_files);
    if (cnt < 0) {
        pmm_free(ents);
        printf("{FG(255,0,0)}No such directory\n");
        return -1;
    }
    printf("{FG(0,255,255)}=== %s ===\n", norm);
    if (cnt == 0) { pmm_free(ents); printf("{FG(255,165,0)}Empty\n"); return 0; }
    uint32_t dirs = 0, files = 0, tsz = 0;
    for (int i = 0; i < cnt; i++) {
        xcfs_dirent_t* d = &ents[i];
        if (d->type == XCFS_TYPE_DIR) {
            printf("{FG(100,200,255)}[DIR]{FG(255,255,255)}  %s\n", d->name);
            dirs++;
        } else {
            uint32_t sv = d->size;
            const char* su = "B";
            if      (sv >= 1024*1024) { sv /= 1024*1024; su = "MB"; }
            else if (sv >= 1024)      { sv /= 1024;      su = "KB"; }
            if (d->flags & XCFS_FLAG_EXECUTABLE) {
                printf("{FG(0,255,0)}%s {FG(150,150,150)}(%u %s){FG(255,255,255)}\n",
                       d->name, sv, su);
            } else {
                printf("{FG(255,255,255)}%s {FG(150,150,150)}(%u %s)\n",
                       d->name, sv, su);
            }
            files++;
            tsz += d->size;
        }
    }
    uint32_t sv = tsz; const char* su = "B";
    if      (sv >= 1024*1024) { sv /= 1024*1024; su = "MB"; }
    else if (sv >= 1024)      { sv /= 1024;      su = "KB"; }
    printf("{FG(0,255,0)}%u dirs %u files (%u %s)\n", dirs, files, sv, su);
    pmm_free(ents);
    return 0;
}

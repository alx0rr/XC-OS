#!/usr/bin/env python3
import sys, os, struct, time, argparse

SEC        = 512
XCFS_MAGIC = 0x58434653
XCFS_VER   = 2
MAX_PATH   = 248
TYPE_FILE  = 0x01
TYPE_DIR   = 0x02
FLAG_PROT  = 0x04
FLAG_EXEC  = 0x08
FLAG_RO    = 0x10

HDR_FMT = "<IIII496x"
ENT_FMT = "<248sIIIBBHIII236x"
assert struct.calcsize(HDR_FMT) == 512
assert struct.calcsize(ENT_FMT) == 512
EPSec = 1

def die(m):  print(f"[ERR] {m}", file=sys.stderr); sys.exit(1)
def info(m): print(f"[   ] {m}")
def ok(m):   print(f"[ OK] {m}")
def warn(m): print(f"[WRN] {m}")

def sec_off(s):    return s * SEC
def read_sec(f, s):
    f.seek(sec_off(s))
    d = f.read(SEC)
    if len(d) < SEC: die(f"short read at sector {s}")
    return d
def write_sec(f, s, d):
    assert len(d) == SEC
    f.seek(sec_off(s))
    f.write(d)

def load_cfg(p):
    if not os.path.isfile(p): die(f"config not found: {p}")
    cfg = {}
    with open(p) as f:
        for ln in f:
            ln = ln.strip()
            if not ln or '=' not in ln: continue
            k, v = ln.split('=', 1)
            cfg[k.strip()] = v.strip()
    for n in ('XCFS_META_SECTOR', 'XCFS_META_SECTORS', 'XCFS_DATA_SECTOR'):
        if n not in cfg: die(f"config missing: {n}")
    return {k: int(v) for k, v in cfg.items()}

def scan_magic(f, img_size, hint):
    for s in ([hint] + [x for x in (1, 2, 32, 64) if x != hint]):
        if (s + 1) * SEC > img_size: continue
        f.seek(s * SEC)
        if struct.unpack_from("<I", f.read(4))[0] == XCFS_MAGIC:
            if s != hint: warn(f"magic at sector {s}, config said {hint}")
            return s
    for s in range(img_size // SEC):
        f.seek(s * SEC)
        r = f.read(4)
        if len(r) < 4: break
        if struct.unpack_from("<I", r)[0] == XCFS_MAGIC:
            warn(f"scanned magic at sector {s}")
            return s
    return None

class XCFS:
    def __init__(self, img, cfg):
        if not os.path.isfile(img): die(f"image not found: {img}")
        self.f        = open(img, 'r+b')
        self.img_size = os.path.getsize(img)
        self.META_N   = cfg['XCFS_META_SECTORS']
        self.DATA_SEC = cfg['XCFS_DATA_SECTOR']
        info(f"image: {self.img_size // 1024 // 1024} MB")
        ms = scan_magic(self.f, self.img_size, cfg['XCFS_META_SECTOR'])
        if ms is None: die("XCFS magic not found — is the image formatted?")
        self.META_SEC = ms
        self._load_hdr()
        self._chk_hdr()
        self._load_ents()
        self._load_bmp()

    def _load_hdr(self):
        raw = read_sec(self.f, self.META_SEC)
        self.magic, self.ver, self.total_s, self.fc = struct.unpack_from(HDR_FMT, raw)

    def _chk_hdr(self):
        if self.magic   != XCFS_MAGIC: die(f"bad magic: 0x{self.magic:08X}")
        if self.ver     != XCFS_VER:   die(f"unsupported version {self.ver}")
        real_secs = self.img_size // SEC
        if self.total_s > real_secs:
            warn(f"header total_sectors={self.total_s} > image sectors={real_secs}, clamping")
            self.total_s = real_secs
        if self.fc      >  self.META_N: die(f"file_count {self.fc} > META_N {self.META_N}")
        if self.DATA_SEC >= self.total_s: die("DATA_SEC >= total_sectors")
        ok(f"header v{self.ver}, {self.total_s} sectors, {self.fc} entries")

    def _save_hdr(self):
        raw = bytearray(SEC)
        struct.pack_into(HDR_FMT, raw, 0, self.magic, self.ver, self.total_s, self.fc)
        write_sec(self.f, self.META_SEC, bytes(raw))

    def _load_ents(self):
        nsec = max(1, min((self.fc + EPSec - 1) or 1, self.META_N))
        self.ents = []
        for si in range(nsec):
            raw = read_sec(self.f, self.META_SEC + 1 + si)
            for ei in range(EPSec):
                pb,ss,sz,pi,tp,fl,cc,fci,cr,mo = struct.unpack_from(ENT_FMT, raw, ei*struct.calcsize(ENT_FMT))
                self.ents.append({'path': pb.rstrip(b'\x00').decode('utf-8','replace'),
                    'start':ss,'size':sz,'parent':pi,'type':tp,'flags':fl,
                    'cc':cc,'fc':fci,'cr':cr,'mo':mo})
        info(f"loaded {self.fc} entries ({len(self.ents)} slots)")

    def _save_ent(self, idx):
        e   = self.ents[idx]
        s   = self.META_SEC + 1 + idx // EPSec
        raw = bytearray(read_sec(self.f, s))
        off = (idx % EPSec) * struct.calcsize(ENT_FMT)
        pb  = e['path'].encode()[:MAX_PATH-1].ljust(MAX_PATH, b'\x00')
        struct.pack_into(ENT_FMT, raw, off,
            pb,e['start'],e['size'],e['parent'],e['type'],e['flags'],e['cc'],e['fc'],e['cr'],e['mo'])
        write_sec(self.f, s, bytes(raw))

    def _bmp_n(self):    return (self.total_s + 8*SEC - 1) // (8*SEC)
    def _bmp_sec(self):  return self.META_SEC + 1 + self.META_N

    def _load_bmp(self):
        self.bmp = bytearray()
        for i in range(self._bmp_n()):
            self.bmp += read_sec(self.f, self._bmp_sec() + i)
        info(f"bitmap: {self._bmp_n()} sectors")

    def _save_bmp(self):
        for i in range(self._bmp_n()):
            write_sec(self.f, self._bmp_sec() + i,
                      bytes(self.bmp[i*SEC:(i+1)*SEC].ljust(SEC, b'\x00')))

    def _tst(self, s):
        if s < self.DATA_SEC: return True
        idx = s - self.DATA_SEC; by,bi = idx>>3, idx&7
        return by < len(self.bmp) and bool((self.bmp[by]>>bi)&1)

    def _set(self, s, v):
        if s < self.DATA_SEC: return
        idx = s - self.DATA_SEC; by,bi = idx>>3, idx&7
        if by >= len(self.bmp): return
        if v: self.bmp[by] |=  (1<<bi)
        else: self.bmp[by] &= ~(1<<bi) & 0xFF

    def _mark(self, s, n, v):
        for i in range(n): self._set(s+i, v)

    def _find_free(self, n):
        total = self.total_s - self.DATA_SEC
        run = start = 0
        for i in range(total):
            by,bi = i>>3, i&7
            if by >= len(self.bmp): break
            if not ((self.bmp[by]>>bi)&1):
                if run == 0: start = i
                run += 1
                if run >= n: return self.DATA_SEC + start
            else: run = 0
        return None

    def check(self):
        info("integrity check...")
        errs = 0
        if not self.ents or self.ents[0]['path'] != '/':
            warn("entry[0] is not '/'"); errs += 1
        seen = {}
        for i, e in enumerate(self.ents[:self.fc]):
            p = e['path']
            if p in seen: warn(f"duplicate '{p}'"); errs += 1
            seen[p] = i
            if e['type'] == TYPE_FILE:
                ns = max(1,(e['size']+SEC-1)//SEC); ss = e['start']
                if ss < self.DATA_SEC: warn(f"'{p}': start {ss} < DATA_SEC"); errs+=1; continue
                if ss+ns > self.total_s: warn(f"'{p}': overflow"); errs+=1
                for si in range(ns):
                    if not self._tst(ss+si): warn(f"'{p}': sector {ss+si} not in bitmap"); errs+=1
        if errs: die(f"{errs} integrity error(s)")
        else: ok("integrity ok")

    def _norm(self, p):
        if not p.startswith('/'): p = '/'+p
        parts = []
        for seg in p.split('/'):
            if seg in ('','.'):  continue
            if seg == '..':
                if parts: parts.pop()
            else: parts.append(seg)
        return '/'+'/'.join(parts)

    def _find(self, p):
        for i,e in enumerate(self.ents[:self.fc]):
            if e['path'] == p: return i
        return -1

    def _par(self, norm):
        if norm == '/': return 0
        par = norm.rsplit('/',1)[0] or '/'
        idx = self._find(par)
        if idx < 0: die(f"parent '{par}' not found")
        if self.ents[idx]['type'] != TYPE_DIR: die(f"'{par}' not a dir")
        return idx

    def _alloc(self, n):
        ss = self._find_free(n)
        if ss is None: die(f"no space for {n} sectors")
        if ss+n > self.total_s: die("allocation out of bounds")
        self._mark(ss, n, 1)
        info(f"allocated {n} sectors @ {ss}")
        return ss

    def _write_data(self, ss, data, sz):
        ns = max(1,(sz+SEC-1)//SEC)
        for si in range(ns):
            chunk = data[si*SEC:(si+1)*SEC].ljust(SEC, b'\x00')
            if ss+si >= self.total_s: die(f"sector {ss+si} out of bounds")
            write_sec(self.f, ss+si, chunk)
        info(f"wrote {sz} B ({ns} sectors) @ {ss}")

    def inject(self, host, xcfs_path, flags=0):
        norm = self._norm(xcfs_path)
        if len(norm.encode()) >= MAX_PATH: die(f"path too long")
        with open(host,'rb') as hf: data = hf.read()
        sz = len(data); ns = max(1,(sz+SEC-1)//SEC)
        info(f"{host} -> {norm} ({sz} B, {ns} sectors)")
        ex = self._find(norm)
        if ex >= 0: self._overwrite(ex, norm, data, sz, ns, flags)
        else:       self._create(norm, data, sz, ns, flags)
        self.f.flush()
        ok(f"'{norm}' injected")

    def _overwrite(self, idx, norm, data, sz, ns, flags):
        e = self.ents[idx]
        if e['flags'] & FLAG_PROT: die(f"'{norm}' protected")
        if e['flags'] & FLAG_RO:   die(f"'{norm}' read-only")
        if e['type']  != TYPE_FILE: die(f"'{norm}' is a dir")
        old_ss = e['start']; old_ns = max(1,(e['size']+SEC-1)//SEC)
        info(f"overwrite idx {idx}, was {old_ns} sectors @ {old_ss}")
        if ns <= old_ns:
            self._write_data(old_ss, data, sz)
            e['size']=sz; e['mo']=int(time.time())&0xFFFFFFFF
            if flags: e['flags']=(e['flags']&FLAG_PROT)|(flags&~FLAG_PROT)
            self._save_ent(idx)
        else:
            new_ss = self._alloc(ns)
            self._write_data(new_ss, data, sz)
            self._mark(old_ss, old_ns, 0)
            e['start']=new_ss; e['size']=sz; e['mo']=int(time.time())&0xFFFFFFFF
            if flags: e['flags']=(e['flags']&FLAG_PROT)|(flags&~FLAG_PROT)
            self._save_ent(idx); self._save_bmp()

    def _create(self, norm, data, sz, ns, flags):
        if self.fc >= self.META_N: die("entry table full")
        par = self._par(norm); ss = self._alloc(ns)
        self._write_data(ss, data, sz)
        now = int(time.time())&0xFFFFFFFF
        e = {'path':norm,'start':ss,'size':sz,'parent':par,'type':TYPE_FILE,
             'flags':flags&~FLAG_PROT,'cc':0,'fc':0,'cr':now,'mo':now}
        idx = self.fc
        if idx < len(self.ents): self.ents[idx] = e
        else: self.ents.append(e)
        self.fc += 1
        self._save_ent(idx); self._save_hdr(); self._save_bmp()
        ok(f"created [{idx}] '{norm}' @ {ss}")

    def ls(self, path='/'):
        norm = self._norm(path)
        if self._find(norm) < 0: die(f"'{norm}' not found")
        print(f"\n{norm}:")
        for e in self.ents[:self.fc]:
            p = e['path']
            if p == norm: continue
            par = p.rsplit('/',1)[0] or '/'
            if par != norm: continue
            tp = 'd' if e['type']==TYPE_DIR else 'f'
            fl = ('x' if e['flags']&FLAG_EXEC else '-') + \
                 ('r' if e['flags']&FLAG_RO   else '-') + \
                 ('p' if e['flags']&FLAG_PROT else '-')
            print(f"  [{tp}{fl}]  {p.rsplit('/',1)[-1]:40s}  {e['size']:>8} B")

    def close(self): self.f.flush(); self.f.close()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("img")
    ap.add_argument("--cfg",   default=None)
    ap.add_argument("--exec",  action="store_true")
    ap.add_argument("--ro",    action="store_true")
    ap.add_argument("--ls",    nargs='?', const='/', metavar='PATH')
    ap.add_argument("--check", action="store_true")
    a, rest = ap.parse_known_args()
    a.host_file = rest[0] if len(rest) > 0 else None
    a.xcfs_path = rest[1] if len(rest) > 1 else None

    cp = a.cfg or os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(a.img)), '..', 'config.cfg'))
    cfg = load_cfg(cp)

    fs = XCFS(a.img, cfg)
    fs.check()

    if a.check:               fs.close(); return
    if a.ls is not None:      fs.ls(a.ls); fs.close(); return

    if not a.host_file or not a.xcfs_path:
        ap.error("host_file and xcfs_path required")
    if not os.path.isfile(a.host_file):
        die(f"host file not found: {a.host_file}")

    flags = (FLAG_EXEC if a.exec else 0) | (FLAG_RO if a.ro else 0)
    fs.inject(a.host_file, a.xcfs_path, flags)
    fs.close()

if __name__ == "__main__":
    main()


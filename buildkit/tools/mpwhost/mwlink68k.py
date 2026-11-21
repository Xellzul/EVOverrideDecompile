"""LinkEmu: cwemu's Unicorn PPC host with the Resource Manager subset a CodeWarrior 68K linker needs to write an
application (the CW Pro 3 CD's MPW MWLink68K imports the same Toolbox calls as its IDE plugin; machost/mwlink68k3.py
runs the plugin on this class).  Mac paths map to host paths as in cwemu.  The output application's resource
fork is written as a plain fork file `<out>.rsrc` beside an (empty) data fork `<out>`; types/creators are
recorded in `<out>.finf`."""
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cwemu  # noqa: E402
from cwemu import u32, s32  # noqa: E402



class Res:
    def __init__(self, typ, rid, name, attrs, data):
        self.typ, self.id, self.name, self.attrs, self.data = typ, rid, name, attrs, data


class ResFile:
    def __init__(self, path):
        self.path = path
        self.res = []      # in creation order
        self.attrs = 0
        if os.path.exists(path) and os.path.getsize(path):
            self.parse(open(path, "rb").read())

    def parse(self, d):
        doff, moff, dlen, mlen = struct.unpack_from(">IIII", d, 0)
        m = d[moff:moff + mlen]
        self.attrs = struct.unpack_from(">H", m, 22)[0]
        tl, nl = struct.unpack_from(">HH", m, 24)
        nt = (struct.unpack_from(">H", m, tl)[0] + 1) & 0xffff
        for i in range(nt):
            typ, cnt, ref = struct.unpack_from(">4sHH", m, tl + 2 + 8 * i)
            for j in range(cnt + 1):
                rid, no, aoff = struct.unpack_from(">hHI", m, tl + ref + 12 * j)
                attrs, off = aoff >> 24, aoff & 0xffffff
                n = struct.unpack_from(">I", d, doff + off)[0]
                name = None if no == 0xffff else m[nl + no + 1:nl + no + 1 + m[nl + no]]
                self.res.append(Res(typ, rid, name, attrs, d[doff + off + 4:doff + off + 4 + n]))

    def find(self, typ, rid):
        for r in self.res:
            if r.typ == typ and r.id == rid:
                return r
        return None

    def write(self):
        data = bytearray()
        types = []
        for r in self.res:
            if r.typ not in types:
                types.append(r.typ)
        names = bytearray()
        reflists = []
        offs = {}
        for r in self.res:
            offs[id(r)] = len(data)
            data += struct.pack(">I", len(r.data)) + r.data
            while len(data) % 4:
                data += b"\0"
        tl = bytearray(struct.pack(">H", (len(types) - 1) & 0xffff))
        refs = bytearray()
        base = 2 + 8 * len(types)
        for t in types:
            rs = [r for r in self.res if r.typ == t]
            tl += struct.pack(">4sHH", t, len(rs) - 1, base + len(refs))
            for r in rs:
                if r.name is None:
                    no = 0xffff
                else:
                    no = len(names)
                    names += bytes([len(r.name)]) + r.name
                refs += struct.pack(">hHII", r.id, no, (r.attrs << 24) | offs[id(r)], 0)
        m = bytearray(16 + 4 + 2 + 2 + 2 + 2) + tl + refs
        nameoff = len(m)
        m += names
        hdr = struct.pack(">IIII", 256, 256 + len(data), len(data), len(m))
        m[0:16] = hdr
        struct.pack_into(">HHH", m, 22, self.attrs, 28, nameoff)
        open(self.path, "wb").write(hdr + bytes(240) + data + m)


class LinkEmu(cwemu.Emu):
    def __init__(self, path, args):
        super().__init__(path, args)
        self.rfiles = {}       # refnum -> ResFile
        self.nextref = 1000
        self.cur = None
        self.handles = {}      # handle -> Res
        self.reserr = 0

    # ---- volumes / misc
    def f_HGetVol(self, name, vref, dirid, *a):
        if name:
            self.wr(name, b"\3Vol")
        if vref:
            self.w16(vref, 0xffff)
        if dirid:
            self.w32(dirid, self.dirid(os.getcwd()))
        return 0

    # ---- Resource Manager (write side)
    def rpath(self, p):
        return p + ".rsrc"

    def openres(self, p, create=False):
        rp = self.rpath(p)
        for ref, f in self.rfiles.items():
            if f.path == rp:
                self.cur = ref
                return ref
        if not create and not os.path.exists(rp) and not os.path.exists(p):
            return None
        ref = self.nextref
        self.nextref += 1
        self.rfiles[ref] = ResFile(rp)
        self.cur = ref
        return ref

    def f_ResError(self, *a):
        return u32(self.reserr)

    def f_FSpCreateResFile(self, spec, creator, ftype, script, *a):
        p = self.spec_path(spec)
        if not os.path.exists(p):
            open(p, "wb").close()
        open(p + ".finf", "wb").write(struct.pack(">II", ftype, creator))
        rp = self.rpath(p)
        if not os.path.exists(rp):
            ResFile(rp).write()
        self.reserr = 0
        return 0

    def f_FSpOpenResFile(self, spec, perm, *a):
        p = self.spec_path(spec)
        ref = self.openres(p)
        if ref is None:
            self.reserr = -43
            return u32(-1)
        self.reserr = 0
        return ref

    def f_CurResFile(self, *a):
        return self.cur or 0

    def f_UseResFile(self, ref, *a):
        ref &= 0xffff
        if ref in self.rfiles:
            self.cur = ref
        self.reserr = 0
        return 0

    def f_CloseResFile(self, ref, *a):
        ref &= 0xffff
        f = self.rfiles.pop(ref, None)
        if f:
            self.sync_handles(f)
            f.write()
        self.reserr = 0
        return 0

    def f_UpdateResFile(self, ref, *a):
        f = self.rfiles.get(ref & 0xffff)
        if f:
            self.sync_handles(f)
            f.write()
        self.reserr = 0
        return 0

    def sync_handles(self, f):
        for h, r in self.handles.items():
            if r in f.res:
                r.data = self.handle_bytes(h)

    def handle_bytes(self, h):
        p = self.r32(h)
        return self.rd(p, self.f_GetHandleSize(h)) if p else b""

    def f_SetResLoad(self, *a):
        return 0

    def f_AddResource(self, h, typ, rid, name, *a):
        f = self.rfiles[self.cur]
        rid = s32(rid) if rid & 0x8000 == 0 else (rid & 0xffff) - 0x10000
        rid = ((rid + 0x8000) & 0xffff) - 0x8000
        tb = struct.pack(">I", typ)
        nm = self.pstr(name) if name else b""
        old = f.find(tb, rid)
        if old:
            f.res.remove(old)
        r = Res(tb, rid, nm if nm else None, 0, self.handle_bytes(h))
        f.res.append(r)
        self.handles[h] = r
        self.reserr = 0
        return 0

    def find_res(self, typ, rid, one=False):
        refs = [self.cur] if one else [self.cur] + [k for k in self.rfiles if k != self.cur]
        for ref in refs:
            f = self.rfiles.get(ref)
            r = f.find(typ, rid) if f else None
            if r:
                return r
        return None

    def res_handle(self, r):
        for h, x in self.handles.items():
            if x is r:
                return h
        h = self.newh(len(r.data))
        self.wr(self.r32(h), r.data)
        self.handles[h] = r
        return h

    def f_Get1Resource(self, typ, rid, *a):
        rid = ((rid + 0x8000) & 0xffff) - 0x8000
        r = self.find_res(struct.pack(">I", typ), rid, True)
        if r:
            self.reserr = 0
            return self.res_handle(r)
        if not self.rfiles or self.cur not in self.rfiles:
            return super().f_Get1Resource(typ, rid)
        self.reserr = -192
        return 0

    def f_GetResource(self, typ, rid, *a):
        rid = ((rid + 0x8000) & 0xffff) - 0x8000
        r = self.find_res(struct.pack(">I", typ), rid)
        if r:
            self.reserr = 0
            return self.res_handle(r)
        return super().f_GetResource(typ, rid)

    def f_RemoveResource(self, h, *a):
        r = self.handles.pop(h, None)
        for f in self.rfiles.values():
            if r in f.res:
                f.res.remove(r)
        self.reserr = 0
        return 0

    def f_ChangedResource(self, h, *a):
        self.reserr = 0
        return 0

    def f_WriteResource(self, h, *a):
        r = self.handles.get(h)
        if r:
            r.data = self.handle_bytes(h)
        self.reserr = 0
        return 0

    def f_DetachResource(self, h, *a):
        r = self.handles.pop(h, None)
        if r:
            r.data = self.handle_bytes(h)
        self.reserr = 0
        return 0

    def f_ReleaseResource(self, h, *a):
        self.f_DetachResource(h)
        return 0

    def f_SetResAttrs(self, h, attrs, *a):
        r = self.handles.get(h)
        if r:
            r.attrs = attrs & 0xff
        self.reserr = 0
        return 0

    def f_GetResAttrs(self, h, *a):
        r = self.handles.get(h)
        return r.attrs if r else 0

    def f_SetResInfo(self, h, rid, name, *a):
        r = self.handles.get(h)
        if r:
            r.id = ((rid + 0x8000) & 0xffff) - 0x8000
            if name:
                r.name = self.pstr(name) or None
        return 0

    def f_SetResFileAttrs(self, ref, attrs, *a):
        f = self.rfiles.get(ref & 0xffff)
        if f:
            f.attrs = attrs & 0xffff
        return 0

    # ---- Finder info: the linker classifies its inputs by file type
    def finfo(self, p):
        try:
            head = open(p, "rb").read(4)
        except OSError:
            head = b""
        if os.path.exists(p + ".finf"):
            return open(p + ".finf", "rb").read(8) + bytes(8)
        if head == b"MWOB":
            return b"MPLFCWIE" + bytes(8)
        if head[:2] == bytes([1, 0]):
            return b"OBJ MPS " + bytes(8)
        return b"TEXTMPS " + bytes(8)

    def f_HGetFInfo(self, vol, did, name, fi, *a):
        p = self.mpd(did, name)
        if not os.path.exists(p):
            return u32(-43)
        self.wr(fi, self.finfo(p))
        return 0

    def f_PBHGetFInfoSync(self, pb, *a):
        p = self.full(self.r32(pb + 48), self.pstr(self.r32(pb + 18)))
        if not os.path.isfile(p):
            self.w16(pb + 16, 0xffd5)
            return u32(-43)
        self.wr(pb + 32, self.finfo(p))
        self.w32(pb + 54, os.path.getsize(p))
        rp = self.rpath(p)
        self.w32(pb + 64, os.path.getsize(rp) if os.path.exists(rp) else 0)
        self.w16(pb + 16, 0)
        return 0

    def f_FSpGetFInfo(self, spec, fi, *a):
        p = self.spec_path(spec)
        if not os.path.exists(p):
            return u32(-43)
        self.wr(fi, self.finfo(p))
        return 0

    # ---- the rest of the linker's imports
    def f_HCreateResFile(self, vol, did, name, *a):
        p = self.mpd(did, name)
        if not os.path.exists(p):
            open(p, "wb").close()
        if not os.path.exists(self.rpath(p)):
            ResFile(self.rpath(p)).write()
        self.reserr = 0
        return 0

    def f_HOpenResFile(self, vol, did, name, perm, *a):
        p = self.mpd(did, name)
        ref = self.openres(p)
        if ref is None:
            self.reserr = -43
            return u32(-1)
        self.reserr = 0
        return ref

    def f_HOpenRF(self, vol, did, name, perm, refp, *a):
        p = self.rpath(self.mpd(did, name))
        if perm & 3 == 1:
            if not os.path.isfile(p):
                return u32(-43)
            f = open(p, "rb")
        else:
            f = open(p, "r+b" if os.path.exists(p) else "w+b")
        self.w16(refp, self.newfd(f))
        return 0

    def f_Count1Resources(self, typ, *a):
        f = self.rfiles.get(self.cur)
        t = struct.pack(">I", typ)
        return sum(1 for r in f.res if r.typ == t) if f else 0

    def f_Get1IndResource(self, typ, idx, *a):
        f = self.rfiles.get(self.cur)
        t = struct.pack(">I", typ)
        rs = [r for r in f.res if r.typ == t] if f else []
        if 1 <= idx <= len(rs):
            return self.res_handle(rs[idx - 1])
        self.reserr = -192
        return 0

    def f_FSpDelete(self, spec, *a):
        p = self.spec_path(spec)
        ok = False
        for q in (p, p + ".rsrc", p + ".finf"):
            if os.path.isfile(q):
                os.remove(q)
                ok = True
        return 0 if ok else u32(-43)

    def f_FSpDirCreate(self, spec, script, did, *a):
        p = self.spec_path(spec)
        os.makedirs(p, exist_ok=True)
        if did:
            self.w32(did, self.dirid(p))
        return 0

    def f_PBWriteSync(self, pb, *a):
        fd = self.r16(pb + 24)
        buf, cnt, mode, off = self.r32(pb + 32), self.r32(pb + 36), self.r16(pb + 44), self.r32(pb + 46)
        f = self.fds[fd]
        if mode & 3 == 1:
            f.seek(off)
        elif mode & 3 == 2:
            f.seek(s32(off), 2)
        elif mode & 3 == 3:
            f.seek(s32(off), 1)
        f.write(self.rd(buf, cnt))
        self.w32(pb + 40, cnt)
        self.w16(pb + 16, 0)
        return 0

    def f_PBHSetFInfoSync(self, pb, *a):
        self.w16(pb + 16, 0)
        return 0

    def f_FlushVol(self, *a): return 0
    def f_HNoPurge(self, *a): return 0
    def f_HPurge(self, *a): return 0
    def f_LMGetResLoad(self, *a): return 1
    def f_LMSetResLoad(self, *a): return 0
    def f_LMGetBootDrive(self, *a): return 0xffff

    def f_DebugStr(self, s, *a):
        raise RuntimeError("DebugStr %r" % self.pstr(s))

    def f_BlockMoveData(self, s, d, n, *a):
        return self.f_BlockMove(s, d, n)

    def f_strstr(self, h, n, *a):
        hs, ns = self.cstr(h), self.cstr(n)
        i = hs.find(ns)
        return 0 if i < 0 else h + i

    def f_vsprintf(self, buf, fmt, ap, *a):
        ni, nd = self.va_list(ap)
        s = self.fmt(self.cstr(fmt), ni, nd)
        self.wr(buf, s + bytes(1))
        return len(s)

    def f_sscanf(self, s, fmt, *a):
        # integer conversions only (MWLinkPPC 1.x reads -stacksize and the like with "%lu")
        import re
        src, f = self.cstr(s).decode("latin-1"), self.cstr(fmt).decode("latin-1")
        convs = re.findall(r"%(l|h)?([dux])", f)
        if not convs or re.sub(r"%(l|h)?[dux]|\s", "", f):
            raise RuntimeError("sscanf %r %r" % (src, f))
        ni, _ = self.varargs_regs(5)
        done = 0
        for size, conv in convs:
            m = re.match(r"\s*([-+]?[0-9a-fA-F]+)" if conv == "x" else r"\s*([-+]?\d+)", src)
            if not m:
                break
            src = src[m.end():]
            v = u32(int(m.group(1), 16 if conv == "x" else 10))
            (self.w16 if size == "h" else self.w32)(ni(), v & (0xffff if size == "h" else 0xffffffff))
            done += 1
        return done


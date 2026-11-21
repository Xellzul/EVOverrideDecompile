#!/usr/bin/env python3
"""mwppc3.py -- run the CW Pro 3 Mac compiler plugins (PEF/PPC, API of CW IDE 3.0) under Unicorn.
usage: mwppc3.py --plugin <base> [--panels name=hex ...] [--cb-verbose] [-i dir]... -o obj src
<base> + .data/.rsrc are the plugin forks.  Based on the sibling's mwppcemu (Pro 6 plugin API)."""
import os, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mwppcemu_base as B
from mwppcemu_base import *
from fractions import Fraction
import sane

CTX_VERSION = 8
# callback tables: (table field offset in ctx) -> {slot offset: name}
SLOTS = {(0x104, 0x30): "GetNamedPreferences", (0x104, 0x6c): "PeekHandle",
         (0x266, 0x10): "DisplayLines", (0x104, 0x20): "ReportMessage", (0x104, 0x68): "PeekHandle", (0x104, 0xc): "FreeMemHandle", (0x266, 8): "StoreObjectData", (0x104, 4): "FindAndLoadFile", (0x104, 8): "GetFileText"}


class Host3(B.Host):
    def __init__(self, base, o, panels):
        self.panels = panels
        B_init(self, base, o)

    def load(self):
        B.Host.load(self)
        uc = self.uc
        self.ctx = self.malloc(0x400)
        uc.mem_write(self.ctx, bytes(0x400))
        self.w32(self.ctx + 4, CTX_VERSION)
        self.w32(self.ctx + 0x9c, 0x43574945)
        self.w32(self.ctx + 0xa0, 0x436f6d70)
        self.tables = {}
        for k, toff in enumerate((0x104, 0x266, 0x2de)):
            t = self.malloc(0x200)
            for s in range(0, 0x200, 4):
                self.w32(t + s, 0xC0000000 | (toff << 8) | (s >> 2))
            self.w32(self.ctx + toff, t)
            self.tables[toff] = t

    def run(self):
        if self.init_tv:
            block = self.malloc(64)
            self.call(self.init_tv, block)
        for req in (-2, 0, -1):
            self.request = req
            if req == 0:
                t, n = self.load_text(self.o.source)
                self.included.add(self.o.source.lower())
                self.spec_from_path(self.ctx + 0x10c, self.o.source)
                self.w32(self.ctx + 0x152, t)
                self.w32(self.ctx + 0x156, n)
                if getattr(self.o, 'symmode', False):
                    self.uc.mem_write(self.ctx + 0x15e, bytes([1]))
            self.w32(self.ctx, req & 0xffffffff)
            r = self.call(self.main_tv, self.ctx)
            print("[emu] request", req, "->", r, file=sys.stderr)
            if req == -2 and r:
                raise CompileFailed("plugin initialisation returned %d" % r)
            if req == 0:
                res = r
        return res

    # Toolbox
    def rsrc_find(self, typ, rid):
        fork = self.rsrc
        data, mapo = struct.unpack_from(">II", fork, 0)
        tl = mapo + struct.unpack_from(">H", fork, mapo + 24)[0]
        for t in range(struct.unpack_from(">H", fork, tl)[0] + 1):
            ty, cnt, ref = struct.unpack_from(">4sHH", fork, tl + 2 + 8 * t)
            if ty != typ:
                continue
            for k in range(cnt + 1):
                r = tl + ref + 12 * k
                if struct.unpack_from(">h", fork, r)[0] == rid:
                    off = struct.unpack_from(">I", fork, r + 4)[0] & 0xffffff
                    n = struct.unpack_from(">I", fork, data + off)[0]
                    return fork[data + off + 4:data + off + 4 + n]
        return None

    def f_GetResource(self, typ, rid, *a):
        rid = B._s16(rid)
        d = self.rsrc_find(struct.pack(">I", typ), rid)
        self.reserr = 0 if d is not None else -192
        if d is None:
            return 0
        h = self.hnew(len(d))
        self.uc.mem_write(self.r32(h), d)
        return h

    def f_ResError(self, *a):
        return getattr(self, "reserr", 0) & 0xffffffff

    def f_ReleaseResource(self, h, *a):
        return 0

    def f_MoveHHi(self, *a): return None
    def f_HLock(self, *a): return None
    f_HUnlock = f_HLockHi = f_MoveHHi
    def f_HGetState(self, h, *a): return 0
    def f_HSetState(self, *a): return None
    def f_GetHandleSize(self, h, *a): return self.hsize(h)
    def f_SetHandleSize(self, h, n, *a):
        self.hresize(h, n); self.memerr = 0
    def f_NewHandle(self, n, *a):
        self.memerr = 0
        return self.hnew(n)
    f_TempNewHandle = lambda self, n, perr, *a: (self.w16(perr, 0), self.hnew(n))[1]
    def f_DisposeHandle(self, h, *a):
        self.hfree(h); self.memerr = 0
    def f_NewPtr(self, n, *a):
        self.memerr = 0
        return self.malloc(n)
    def f_DisposePtr(self, p, *a):
        self.mfree(p)
    def f_MemError(self, *a): return getattr(self, "memerr", 0) & 0xffffffff
    f_LMGetMemErr = f_MemError
    def f_BlockMoveData(self, src, dst, n, *a):
        if n: self.uc.mem_write(dst, bytes(self.uc.mem_read(src, n)))
    def f_PtrAndHand(self, p, h, n, *a):
        old = self.hsize(h)
        self.hresize(h, old + n)
        self.uc.mem_write(self.r32(h) + old, bytes(self.uc.mem_read(p, n)))
        return 0
    def f_p2cstr(self, p, *a):
        n = self.uc.mem_read(p, 1)[0]
        d = bytes(self.uc.mem_read(p + 1, n))
        self.uc.mem_write(p, d + b"\0")
        return p
    def f_c2pstr(self, p, *a):
        d = self.cstr(p).encode("mac_roman")[:255]
        self.uc.mem_write(p, bytes([len(d)]) + d)
        return p
    def f_GetIndString(self, buf, sid, idx, *a):
        s = B.str_resource(self.rsrc, B._s16(sid), B._s16(idx))
        self.uc.mem_write(buf, bytes([len(s)]) + s if s is not None else b"\0")
    def f_CharacterByteType(self, *a): return 0
    def f_LMGetTicks(self, *a):
        self.ticks += 1
        return self.ticks

    def f_NewRoutineDescriptor(self, proc, info, isa, *a):
        self.rds = getattr(self, "rds", {})
        self.rds[proc] = info
        return proc
    def f_DisposeRoutineDescriptor(self, *a): return None

    def cb_ReportMessage(self, ctx, ref, l1, l2, level, errno, *a):
        level = B._s16(level)
        if level == 2:
            self.errors += 1
        elif level == 1:
            self.warnings += 1
        msg = self.cstr(l1).replace(chr(13), " ") if l1 else ""
        line = ""
        if ref:
            raw = bytes(self.uc.mem_read(ref, B.FSSPEC + 4))
            line = "%s:%d: " % (raw[7:7 + raw[6]].decode("mac_roman"), struct.unpack_from(">I", raw, B.FSSPEC)[0])
        print("%s%s: %s" % (line, {2: "Error", 1: "Warning"}.get(level, "Note"), msg), flush=True)
        return 0

    def cb_DisplayLines(self, ctx, n, *a):
        return 0

    def cb_FreeMemHandle(self, ctx, mh, *a):
        return 0

    def cb_StoreObjectData(self, ctx, which, od, *a):
        h = self.r32(od)
        obj = bytes(self.uc.mem_read(self.r32(h), self.hsize(h)))
        code = self.r32(od + 12)
        if getattr(self.o, "ppc", False):
            B.write_mwob(self.o.output, self.o.source, obj)
            self.stored = True
            return 0
        self.objinfo = (code, self.r32(od + 16), self.r32(od + 20), self.r32(od + 24))
        base = os.path.basename(self.o.source).encode("mac_roman") + bytes(1)
        doff = (0x30 + len(base) + 3) & ~3
        mdate = (int(os.path.getmtime(self.o.source)) + 2082758400) & 0xffffffff
        hdr = b"MWOBM68K" + struct.pack(">IIIIIIIIII", 0, 2, code, 0, 1, mdate, 0x30, 0, doff, len(obj))
        with open(self.o.output, "wb") as f:
            f.write(hdr + base + bytes(doff - 0x30 - len(base)) + obj)
        self.stored = True
        return 0

    def find_include(self, name, full):
        nm = name.replace("/", os.sep).replace(":", os.sep) if not (len(name) > 1 and name[1] == ":") else name
        if os.path.isabs(nm):
            return nm if os.path.isfile(nm) else None
        for d in (self.o.user if full else []) + self.o.sys_dirs:
            p = B.case_blind(os.path.join(d, nm))
            if os.path.isfile(p):
                return os.path.normpath(p)
        return None

    def cb_FindAndLoadFile(self, ctx, pname, fi, *a):
        return B.Host.f_CWFindAndLoadFile(self, ctx, pname, fi)

    def cb_GetFileText(self, ctx, spec, ptext, plen, ptype, *a):
        return B.Host.f_CWGetFileText(self, ctx, spec, ptext, plen, ptype)

    def cb_PeekHandle(self, ctx, mh, pout, *a):
        self.w32(pout, mh)
        return 0

    def cb_GetNamedPreferences(self, ctx, pname, pout, *a):
        name = self.cstr(pname)
        d = self.panels.get(name)
        if d is None:
            return 2
        h = self.hnew(len(d))
        self.uc.mem_write(self.r32(h), d)
        self.w32(pout, h)
        return 0

    def on_import(self, uc, addr, size, data):
        i = (addr - B.STUB) // 4
        if self.imp_names[i] == "CallUniversalProc":
            upp = uc.reg_read(B.REG(3))
            if (upp >> 28) != 0xC:
                if upp not in getattr(self, "rds", {}):
                    self.failure = "CallUniversalProc: unknown UPP 0x%x" % upp
                    uc.emu_stop()
                    return
                info = uc.reg_read(B.REG(4))
                args = [uc.reg_read(B.REG(5 + k)) for k in range(6)]
                try:
                    r = self.call68k(upp, info, args)
                except Exception as e:
                    self.failure = "68K call: %s" % e
                    uc.emu_stop()
                    return
                uc.reg_write(B.REG(3), r & 0xffffffff)
                return
        B.Host.on_import(self, uc, addr, size, data)

    # ---------------------------------------------------------------- 68K mixed mode (Pascal stack frames)
    def m68k_init(self):
        import unicorn as U
        from unicorn import m68k_const as MC
        self.MC = MC
        u = U.Uc(U.UC_ARCH_M68K, U.UC_MODE_BIG_ENDIAN)
        u.ctl_set_cpu_model(MC.UC_CPU_M68K_M68040)
        u.mem_map_ptr(B.BASE, B.RAM, 7, self.ramaddr)
        self.u68 = u
        self.ret68 = self.malloc(16)
        self.w16(self.ret68, 0x4e75)
        self.stk68 = self.malloc(0x20000) + 0x1ff00
        self.sane_env = 0
        self.sane_flags = 0
        u.hook_add(U.UC_HOOK_CODE, self.on_code68)
        def bad(uc, access, addr, size, value, data):
            self.fail68 = "68K bad access %x at pc %x sp %x" % (addr, uc.reg_read(MC.UC_M68K_REG_PC), uc.reg_read(MC.UC_M68K_REG_A7))
            return False
        u.hook_add(U.UC_HOOK_MEM_INVALID, bad)

    def r16(self, a):
        return struct.unpack(">H", self.uc.mem_read(a, 2))[0]

    def call68k(self, proc, info, args):
        if not hasattr(self, "u68"):
            self.m68k_init()
        u, MC = self.u68, self.MC
        sp = self.stk68

        def push(v, n):
            nonlocal sp
            sp -= n
            self.uc.mem_write(sp, (v & ((1 << (8 * n)) - 1)).to_bytes(n, "big"))
        rs = (info >> 4) & 3
        if rs:
            push(0, 4 if rs == 3 else 2)
        res_at = sp
        for k in range(6):
            ps = (info >> (6 + 2 * k)) & 3
            if ps == 0:
                break
            if ps == 1:
                push(args[k] << 8, 2)
            elif ps == 2:
                push(args[k], 2)
            else:
                push(args[k], 4)
        push(self.ret68, 4)
        u.reg_write(MC.UC_M68K_REG_SR, 0x2000)
        u.reg_write(MC.UC_M68K_REG_A7, sp)
        u.reg_write(MC.UC_M68K_REG_A6, 0)
        self.fail68 = None
        try:
            u.emu_start(proc, self.ret68)
        except Exception as e:
            raise CompileFailed('%s; %s; pc=%x' % (e, self.fail68, u.reg_read(MC.UC_M68K_REG_PC)))
        if self.fail68:
            raise CompileFailed(self.fail68)
        if rs == 0:
            return 0
        if rs == 1:
            return self.uc.mem_read(res_at, 1)[0]
        if rs == 2:
            return self.r16(res_at)
        return self.r32(res_at)

    def on_code68(self, u, addr, size, data):
        w = self.r16(addr)
        if w in (0xa9eb, 0xa9ec, 0xa9ee):
            try:
                self.sane_trap(u, addr, w)
            except Exception as e:
                self.fail68 = "SANE trap at %x: %s" % (addr, e)
                u.emu_stop()

    def sane_trap(self, u, pc, w):
        MC = self.MC
        sp = u.reg_read(MC.UC_M68K_REG_A7)
        op = self.r16(sp)
        sp += 2
        lo = op & 0xff
        ccr = None
        if w == 0xa9eb:
            fmt = op >> 8
            two = lo in (0, 2, 4, 6, 8, 0xa, 0xc, 0xe, 0x10, 9, 0xb, 0x18)
            if two:
                dst, src = self.r32(sp), self.r32(sp + 4)
                n = 8
            else:
                dst, src = self.r32(sp), None
                n = 4
            if lo == 0xe:        # to extended from format
                self.wr_fmt(dst, 0, self.rd_fmt(src, fmt))
            elif lo == 0x10:     # extended to format
                self.wr_fmt(dst, fmt, self.rd_fmt(src, 0))
            elif lo in (0, 2, 4, 6):
                a, b = self.rd_fmt(dst, 0), self.rd_fmt(src, fmt)
                if lo == 2:
                    b = sane.neg(b)
                r = {0: sane.add, 2: sane.add, 4: sane.mul, 6: sane.div}[lo](a, b)
                self.wr_fmt(dst, 0, r)
            elif lo == 8:
                a, b = self.rd_fmt(dst, 0), self.rd_fmt(src, fmt)
                rl = sane.rel(a, b)
                ccr = {0: 0x04, -1: 0x09, 1: 0x00, None: 0x02}[rl]
            elif lo == 0xd:
                self.wr_fmt(dst, 0, sane.neg(self.rd_fmt(dst, 0)))
            elif lo == 0xf:
                v = self.rd_fmt(dst, 0)
                self.wr_fmt(dst, 0, (v[0], 0) + v[2:] if v[0] != 'n' else v)
            elif lo in (0x14, 0x16):
                v = self.rd_fmt(dst, 0)
                if v[0] == 'f':
                    q = v[2]
                    k = sane.rnd_even(q) if lo == 0x14 else (q.numerator // q.denominator)
                    v = ('f', v[1], Fraction(k))
                self.wr_fmt(dst, 0, v)
            elif lo == 9:        # decimal -> extended
                sg = self.r16(src)
                ex = struct.unpack(">h", bytes(self.uc.mem_read(src + 2, 2)))[0]
                ln = self.uc.mem_read(src + 4, 1)[0]
                dg = bytes(self.uc.mem_read(src + 5, ln)).decode("latin1")
                self.wr_fmt(dst, 0, sane.d2b(sg, ex, dg))
            elif lo == 1:
                self.sane_env = self.r16(dst)
            elif lo == 3:
                self.w16(dst, self.sane_flags)
            else:
                raise CompileFailed("FP68K op %x not implemented" % op)
            sp += n
        elif w == 0xa9ee:
            if lo == 4:          # str2dec(cstr, &index, &decimal, &validPrefix)
                vp, d, ix, s = (self.r32(sp + 4 * k) for k in range(4))
                sp += 16
                self.str2dec(s, ix, d, vp)
            else:
                raise CompileFailed("DecStr68K op %x not implemented" % op)
        else:
            raise CompileFailed("Elems68K op %x not implemented" % op)
        u.reg_write(MC.UC_M68K_REG_A7, sp)
        if ccr is not None:
            sr = u.reg_read(MC.UC_M68K_REG_SR)
            u.reg_write(MC.UC_M68K_REG_SR, (sr & ~0x1f) | ccr)
        u.reg_write(MC.UC_M68K_REG_PC, pc + 2)

    def rd_fmt(self, a, f):
        if f == 0:
            return sane.dec_ext(bytes(self.uc.mem_read(a, 10)))
        if f == 0x08:
            return sane.dec_dbl(bytes(self.uc.mem_read(a, 8)))
        if f == 0x10:
            return sane.dec_sgl(bytes(self.uc.mem_read(a, 4)))
        if f == 0x20:
            return sane.from_int(struct.unpack(">h", bytes(self.uc.mem_read(a, 2)))[0])
        if f == 0x28:
            return sane.from_int(struct.unpack(">i", bytes(self.uc.mem_read(a, 4)))[0])
        if f == 0x30:
            return sane.from_int(struct.unpack(">q", bytes(self.uc.mem_read(a, 8)))[0])
        raise CompileFailed("SANE format %x" % f)

    def wr_fmt(self, a, f, v):
        if f == 0:
            self.uc.mem_write(a, sane.enc_ext(v))
        elif f == 0x08:
            self.uc.mem_write(a, sane.enc_dbl(v))
        elif f == 0x10:
            self.uc.mem_write(a, sane.enc_sgl(v))
        elif f == 0x20:
            self.uc.mem_write(a, struct.pack(">h", sane.to_int(v, 16)))
        elif f == 0x28:
            self.uc.mem_write(a, struct.pack(">i", sane.to_int(v, 32)))
        elif f == 0x30:
            self.uc.mem_write(a, struct.pack(">q", sane.to_int(v, 64)))
        else:
            raise CompileFailed("SANE format %x" % f)

    def str2dec(self, s, ixp, dp, vpp):
        txt = bytes(self.uc.mem_read(s, 80)).split(bytes(1))[0].decode("latin1")
        i = int(struct.unpack(">h", bytes(self.uc.mem_read(ixp, 2)))[0])
        j = i
        while j < len(txt) and txt[j] in " " + chr(9):
            j += 1
        sg = 0
        if j < len(txt) and txt[j] in "+-":
            sg = 1 if txt[j] == "-" else 0
            j += 1
        ds, ex, seen, frac = "", 0, False, False
        while j < len(txt) and (txt[j].isdigit() or (txt[j] == "." and not frac)):
            if txt[j] == ".":
                frac = True
            else:
                ds += txt[j]
                seen = True
                if frac:
                    ex -= 1
            j += 1
        valid = seen
        if valid and j < len(txt) and txt[j] in "eE":
            k = j + 1
            es = 1
            if k < len(txt) and txt[k] in "+-":
                es = -1 if txt[k] == "-" else 1
                k += 1
            e0 = k
            while k < len(txt) and txt[k].isdigit():
                k += 1
            if k > e0:
                ex += es * int(txt[e0:k])
                j = k
        ds = ds.lstrip("0")
        while ds.endswith("0"):
            ds = ds[:-1]
            ex += 1
        ds = ds[:20]
        self.w16(dp, sg)
        self.uc.mem_write(dp + 2, struct.pack(">h", ex))
        self.uc.mem_write(dp + 4, bytes([len(ds)]) + ds.encode("latin1"))
        self.w16(ixp, j)
        self.uc.mem_write(vpp, bytes([1 if valid else 0, 0]))

    # CallUniversalProc(upp, procinfo, p1..p6)
    def f_CallUniversalProc(self, upp, info, *a):
        if (upp >> 28) != 0xC:
            raise CompileFailed("CallUniversalProc: bad UPP 0x%x" % upp)
        toff, slot = (upp >> 8) & 0xfff, (upp & 0xff) * 4
        name = SLOTS.get((toff, slot))
        args = list(a)
        if name is None:
            raise CompileFailed("callback %x/%x not mapped, info=%x args=%s" % (toff, slot, info, [hex(x) for x in args[:6]]))
        fn = getattr(self, "cb_" + name)
        return fn(*args)


def B_init(self, base, o):
    self.o = o
    with open(base + ".data", "rb") as f:
        self.pef = B.Pef(f.read())
    with open(base + ".rsrc", "rb") as f:
        self.rsrc = f.read()
    self.licensed = []
    self.uc = B.Uc(B.UC_ARCH_PPC, B.UC_MODE_32 | B.UC_MODE_BIG_ENDIAN)
    self.uc.mem_map(B.STUB, B.STUB_SIZE)
    import mmap, ctypes
    self.mm = mmap.mmap(-1, B.RAM)
    self.ramaddr = ctypes.addressof(ctypes.c_char.from_buffer(self.mm))
    self.uc.mem_map_ptr(B.BASE, B.RAM, 7, self.ramaddr)
    self.errors = self.warnings = 0
    self.stored = False
    self.request = 0
    self.dirs = []
    self.refs = {}
    self.included = set()
    self.heap_top = 0
    self.free = {}
    self.sizes = {}
    self.ticks = 0
    self.failure = None
    self.prefix_ptr = None
    self.load()


def main(argv):
    base = None
    panels = {}
    sysdirs = []
    ppcmode = False
    symmode = False
    args = []
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "--plugin":
            base = argv[i + 1]; i += 2
        elif a == "--panels-from-dump":
            for line in open(argv[i + 1]):
                if len(line) > 24 and line[22:24].isalnum() and all(c in "0123456789abcdef" for c in line[22:].strip()) and line[:22].strip():
                    n, h = line[:22].strip(), bytes.fromhex(line[22:].strip())
                    h = bytearray(h)
                    if n not in ("68K Project", "CFM68K"):
                        h[0], h[1] = h[1], h[0]
                    if n == "C/C++ Compiler":
                        h[0x38], h[0x39] = h[0x39], h[0x38]
                    if n == "68K CodeGen":
                        h[14], h[15] = h[15], h[14]
                    panels[n] = bytes(h)
            i += 2
        elif a == "--sysdirs":
            sysdirs = [x for x in argv[i + 1].split(";") if x]; i += 2
        elif a == "--sym":
            symmode = True; i += 1
        elif a == "--ppc":
            ppcmode = True; i += 1
        else:
            args.append(a); i += 1
    o = Opts(args)
    o.sys_dirs = sysdirs
    o.ppc = ppcmode
    o.symmode = symmode
    h = Host3(base, o, panels)
    t0 = time.time()
    try:
        r = h.run()
    except CompileFailed as e:
        print("mwppc3:", e, file=sys.stderr)
        return 2
    print("[emu] result %d, %d errors, %d warnings, %.1f s" % (r, h.errors, h.warnings, time.time() - t0), file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

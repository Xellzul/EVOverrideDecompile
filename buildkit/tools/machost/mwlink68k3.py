#!/usr/bin/env python3
"""mwlink68k3.py -- run the CW Pro 3 CD's own IDE 68K linker plugin ("MacOS 68K Linker", vers 2.1, PEF/PPC;
toolchain/mwcppc/compilers/Pro30mac/MacOS_68K_Linker.data/.rsrc, unmodified) under Unicorn.

    python -I buildkit/tools/machost/mwlink68k3.py <plugin base> <workdir> <spec.json>

This file stands in for the IDE: it hands the plugin a project (segments = the project's groups, files in link
order, each a compiled source or a library file) and the preference panels, and lets it write the application.
The PEF loader and the File/Resource Manager are mpwhost's (cwemu.Emu, mwlink68k.LinkEmu: the MPW tool is
the same linker and imports the same Toolbox calls); the plugin API is the Pro 3 one of mwppc3.py (a context
struct plus callback tables reached through CallUniversalProc).

spec.json: {"out": name, "segments": [[name, attrs], ...] (ids 1..n; a group with no files keeps its id),
"files": [[name in workdir, segment id, is_library], ...] in link order, "panels": {panel name: hex, Mac layout}}.
A compiled source is given to the plugin as stored object data (the MWOB member of the .o the compiler wrote);
a library (Finder type 'MPLF') is opened by the plugin itself.  The plugin loads every source's object first and
the libraries after them, so a library's data follows all the sources' whatever group it sits in.

What the plugin reads, measured (scratch/LNK68X/b):
- ctx: +0 request (-2 init, 0 link, -1 term), +4 API version 8, +0x10 project FSSpec, +0x56 output directory
  FSSpec, +0x9c 'CWIE', +0xa0 'Link', +0xa4 number of project files, +0x104 / +0x266 callback tables.
- table 0x104: +0 GetFileInfo(ctx, file 0.., checkLocation, CWProjectFileInfo*), +0x10 GetSegmentInfo(ctx,
  segment 1.., {char name[32] (C string); short attributes}*), +0x20 ReportMessage, +0x28 ShowStatus,
  +0x2c UserBreak, +0x30 GetNamedPreferences, +0x68..+0x74 handle attach/detach (identity);
  table 0x266: +4 LoadObjectData(ctx, file, Handle*), +0xc FreeObjectData.
- CWProjectFileInfo: +0 FSSpec, +0x46 moddate, +0x4a segment, +0x4c hasobjectcode, +0x4d hasresources,
  +0x4e isresourcefile, +0x4f weakimport, +0x50 initbefore, +0x52 objmoddate, +0x76 file id, +0x7a file type.
- panels "68K CodeGen", "68K Project", "68K Linker", "CFM68K".  "68K Project" (Mac layout, from the CD's
  "MacOS Toolbox 68K" stationery): +0 version 9, +2 project type (0 application), +4 Str63 output name,
  +0x44 creator, +0x48 type, +0x4c preferred heap (K), +0x50 SIZE flags, +0xb4 minimum heap (K).
"""
import json
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(HERE), "mpwhost"))
import mwlink68k as ML  # noqa: E402
from cwemu import R as REG, BASE, RET, u32, s32  # noqa: E402
from unicorn import UcError  # noqa: E402
from unicorn.ppc_const import UC_PPC_REG_LR, UC_PPC_REG_PC  # noqa: E402

# the "68K Project" panel of the CD's "MacOS Toolbox 68K" stationery (an application), output name replaced
PROJECT = bytes.fromhex(
    "0009000000" + "00" * 63
    + "3f3f3f3f4150504c0000018058c000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
    + "00000000000000000000000000000000000000003f3f3f3f0000000000000000005501900109027b0000000000000000000000000000018000000000"
    + "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000")


class IdeLink(ML.LinkEmu):
    def __init__(self, plugin, spec):
        self.spec = spec
        super().__init__(plugin, [])
        self.errors = 0
        self.ctx = self.alloc(0x400)
        self.wr(self.ctx, bytes(0x400))
        self.w32(self.ctx + 4, 8)
        self.w32(self.ctx + 0x9c, 0x43574945)
        self.w32(self.ctx + 0xa0, 0x4c696e6b)
        self.spec_at(self.ctx + 0x10, os.path.join(os.getcwd(), spec["out"] + ".mcp"))
        self.spec_at(self.ctx + 0x56, os.path.join(os.getcwd(), spec["out"]))
        for toff in (0x104, 0x266, 0x2de):
            t = self.alloc(0x200)
            for s in range(0, 0x200, 4):
                self.w32(t + s, 0xC0000000 | (toff << 8) | (s >> 2))
            self.w32(self.ctx + toff, t)
        self.panels = {k: bytes.fromhex(v) for k, v in spec["panels"].items()}
        nm = spec["out"].encode("mac_roman")
        self.panels.setdefault("68K Project", PROJECT[:4] + bytes([len(nm)]) + nm + bytes(63 - len(nm)) + PROJECT[0x44:])
        self.segments = spec["segments"]
        self.files_ = []
        for name, seg, lib in spec["files"]:
            path = os.path.join(os.getcwd(), name)
            obj = None
            if not lib:   # the MWOB member the compiler stored for this source
                d = open(path, "rb").read()
                if d[:8] != b"MWOBM68K" or struct.unpack_from(">I", d, 0x18)[0] != 1:
                    raise RuntimeError("%s: not a one-member 68K object" % name)
                off, n = struct.unpack_from(">II", d, 0x28)
                obj = d[off:off + n]
            self.files_.append((path, seg, obj))
        self.w32(self.ctx + 0xa4, len(self.files_))

    def init_data(self):    # no StdCLib data imports in a plugin
        pass

    def spec_at(self, a, path):
        nm = os.path.basename(path).encode("mac_roman")[:63]
        self.w16(a, 0xffff)
        self.w32(a + 2, self.dirid(os.path.dirname(path)))
        self.wr(a + 6, bytes([len(nm)]) + nm + bytes(63 - len(nm)))

    def request(self, req):
        uc, p = self.uc, self.pef
        self.w32(self.ctx, u32(req))
        tv = self.bases[p.main_s] + p.main_o
        uc.reg_write(REG(1), self.stack_top - 0x100)
        uc.reg_write(REG(2), self.r32(tv + 4))
        uc.reg_write(REG(3), self.ctx)
        uc.reg_write(UC_PPC_REG_LR, RET)
        try:
            uc.emu_start(self.r32(tv), RET)
        except UcError as e:
            raise RuntimeError("%s; %s; pc=code+0x%x" % (e, self.failure, uc.reg_read(UC_PPC_REG_PC) - BASE))
        if self.failure:
            raise RuntimeError(self.failure)
        v = uc.reg_read(REG(3)) & 0xffff
        return v - 0x10000 if v & 0x8000 else v

    # ---- Toolbox the MPW host lacks or stubs
    def f_NewRoutineDescriptor(self, proc, *a): return proc
    def f_DisposeRoutineDescriptor(self, *a): return None
    def f_LoadResource(self, *a): return None
    def f_ApplicationZone(self, *a): return 0x2000
    def f_HandleZone(self, *a): return 0x2000

    def f_PtrAndHand(self, ptr, h, n, *a):
        old = self.f_GetHandleSize(h)
        d = self.rd(ptr, n)
        self.f_SetHandleSize(h, old + n)
        self.wr(self.r32(h) + old, d)
        return 0

    def f_HandAndHand(self, h1, h2, *a):
        return self.f_PtrAndHand(self.r32(h1), h2, self.f_GetHandleSize(h1))

    # ---- the IDE's callbacks
    def f_CallUniversalProc(self, upp, info, *a):
        if (upp >> 28) != 0xC:
            raise RuntimeError("CallUniversalProc: UPP 0x%x" % upp)
        toff, slot = (upp >> 8) & 0xfff, (upp & 0xff) * 4
        fn = getattr(self, "cb_%x_%x" % (toff, slot), None)
        if fn is None:
            print("[cb] %x/%x info=%x args=%s lr=%x" % (toff, slot, info, [hex(x) for x in a[:6]],
                                                        self.uc.reg_read(UC_PPC_REG_LR) - BASE), file=sys.stderr)
        if fn is None:
            raise RuntimeError("callback %x/%x not implemented" % (toff, slot))
        return fn(*a)

    def cb_104_0(self, ctx, which, check, out, *a):       # GetFileInfo
        which = s32(which)
        if not 0 <= which < len(self.files_):
            return 2
        path, seg, obj = self.files_[which]
        self.wr(out, bytes(0x82))
        self.spec_at(out, path)
        self.w32(out + 0x46, 0xB5000000 + which)
        self.w16(out + 0x4a, seg)
        self.wr(out + 0x4c, bytes([1 if obj is not None else 0]))
        self.w32(out + 0x52, 0xB5000000 + which)
        self.w16(out + 0x76, which + 1)
        self.wr(out + 0x7a, b"MPLF" if obj is None else b"TEXT")
        return 0

    def cb_104_10(self, ctx, which, out, *a):             # GetSegmentInfo
        which = s32(which) - 1
        if not 0 <= which < len(self.segments):
            return 2
        name, attrs = self.segments[which]
        nm = name.encode("mac_roman")[:31]
        self.wr(out, nm + bytes(32 - len(nm)) + struct.pack(">H", attrs))
        return 0

    def cb_104_20(self, ctx, ref, l1, l2, level, errno, *a):   # ReportMessage
        level = s32(u32(level) << 16) >> 16
        self.errors += level == 2
        txt = " ".join(self.cstr(x).decode("mac_roman").replace("\r", " ") for x in (l1, l2) if x)
        print("%s: %s" % ({2: "Error", 1: "Warning"}.get(level, "Note"), txt), flush=True)
        return 0

    def cb_104_28(self, ctx, l1, l2, *a):                 # ShowStatus
        return 0

    def cb_104_2c(self, ctx, *a):                         # UserBreak
        return 0

    def cb_104_30(self, ctx, pname, pout, *a):            # GetNamedPreferences
        d = self.panels.get(self.cstr(pname).decode("mac_roman"))
        if d is None:
            return 2
        h = self.newh(len(d))
        self.wr(self.r32(h), d)
        self.w32(pout, h)
        return 0

    def cb_104_6c(self, ctx, mh, pout, *a):               # handle attach/detach/peek
        self.w32(pout, mh)
        return 0
    cb_104_68 = cb_104_70 = cb_104_74 = cb_104_6c

    def cb_266_4(self, ctx, which, pout, *a):             # LoadObjectData
        obj = self.files_[s32(which)][2]
        if obj is None:
            return 2
        h = self.newh(len(obj))
        self.wr(self.r32(h), obj)
        self.w32(pout, h)
        return 0

    def cb_266_c(self, ctx, which, h, *a):                # FreeObjectData
        return 0


def main():
    plugin, work, specfile = sys.argv[1:4]
    plugin = os.path.abspath(plugin)
    spec = json.load(open(specfile))
    os.chdir(work)
    e = IdeLink(plugin + ".data", spec)
    r = e.request(-2)
    if r == 0:
        r = e.request(0)
        e.request(-1)
    for ref in list(e.rfiles):
        e.f_CloseResFile(ref)
    print("[mwlink68k3] result %d, %d errors" % (r, e.errors), file=sys.stderr)
    sys.exit(1 if r or e.errors else 0)


if __name__ == "__main__":
    main()

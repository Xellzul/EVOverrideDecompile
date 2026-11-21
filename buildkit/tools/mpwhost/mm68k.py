"""Mixed Mode for the MPW MWC68K tools: their `_68K` code resources (Floating Point Glue Code, ScanFloat Glue Code,
...) are 68k code the PPC tool calls through CallUniversalProc.  They run on Unicorn's m68k; the SANE traps
(FP68K, DecStr68K) are modelled by machost/sane.py through the same code the Pro 3 plugin host uses
(mwppc3.Host3: call68k, sane_trap).  The 68k CPU sees the PPC emulator's memory page by page, copied in on first
touch and copied back when the call returns.  Lane EV68CC (EscapeVelocityClassic)."""
import struct
import unicorn as U
from unicorn import m68k_const as MC
import mwppc3

PAGE = 0x1000
PRIV, PRIVSZ = 0x0E000000, 0x40000      # 68k stack and the return stub; outside the PPC emulator's maps


class _Mem:
    """What the borrowed methods use as `self.uc`: reads and writes page the PPC memory in first."""
    def __init__(self, mm):
        self.mm = mm

    def _touch(self, a, n):
        for pg in range(a & ~(PAGE - 1), ((a + max(n, 1) - 1) & ~(PAGE - 1)) + PAGE, PAGE):
            if not (PRIV <= pg < PRIV + PRIVSZ) and not self.mm.page_in(pg):
                raise RuntimeError("68K host access to unmapped %x" % a)

    def mem_read(self, a, n):
        self._touch(a, n)
        return self.mm.u68.mem_read(a, n)

    def mem_write(self, a, d):
        self._touch(a, len(d))
        self.mm.u68.mem_write(a, bytes(d))


class MM68:
    MC = MC
    call68k = mwppc3.Host3.call68k
    on_code68 = mwppc3.Host3.on_code68
    sane_trap = mwppc3.Host3.sane_trap
    rd_fmt = mwppc3.Host3.rd_fmt
    wr_fmt = mwppc3.Host3.wr_fmt
    str2dec = mwppc3.Host3.str2dec

    def __init__(self, ppc):
        self.ppc = ppc
        u = U.Uc(U.UC_ARCH_M68K, U.UC_MODE_BIG_ENDIAN)
        u.ctl_set_cpu_model(MC.UC_CPU_M68K_M68040)
        u.mem_map(PRIV, PRIVSZ)
        self.u68 = u
        self.uc = _Mem(self)            # the borrowed methods read and write memory through self.uc
        self.ret68 = PRIV
        u.mem_write(PRIV, b"\x4e\x75")
        self.stk68 = PRIV + PRIVSZ - 0x100
        self.sane_env = self.sane_flags = 0
        self.fail68 = None
        self.pages = {}
        u.hook_add(U.UC_HOOK_CODE, self.on_code68)
        u.hook_add(U.UC_HOOK_MEM_UNMAPPED, self.on_unmapped)

    def r16(self, a):
        return struct.unpack(">H", self.uc.mem_read(a, 2))[0]

    def r32(self, a):
        return struct.unpack(">I", self.uc.mem_read(a, 4))[0]

    def w16(self, a, v):
        self.uc.mem_write(a, struct.pack(">H", v & 0xffff))

    def page_in(self, pg):
        if pg in self.pages:
            return True
        try:
            d = bytes(self.ppc.mem_read(pg, PAGE))
        except U.UcError:
            return False
        self.u68.mem_map(pg, PAGE)
        self.u68.mem_write(pg, d)
        self.pages[pg] = d
        return True

    def on_unmapped(self, uc, access, addr, size, value, data):
        ok = all(self.page_in(pg) for pg in range(addr & ~(PAGE - 1), ((addr + max(size, 1) - 1) & ~(PAGE - 1)) + PAGE, PAGE))
        if not ok:
            self.fail68 = "68K bad access %x at pc %x" % (addr, uc.reg_read(MC.UC_M68K_REG_PC))
        return ok

    def call(self, proc, info, args):
        for pg in list(self.pages):     # refresh what earlier calls mapped
            d = bytes(self.ppc.mem_read(pg, PAGE))
            self.u68.mem_write(pg, d)
            self.pages[pg] = d
        for pg in (proc & ~(PAGE - 1), (proc & ~(PAGE - 1)) + PAGE):
            self.page_in(pg)
        r = self.call68k(proc, info, list(args) + [0] * 6)
        for pg, old in self.pages.items():
            d = bytes(self.u68.mem_read(pg, PAGE))
            if d != old:
                self.ppc.mem_write(pg, d)
        return r

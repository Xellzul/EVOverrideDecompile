#!/usr/bin/env python3
"""cwemu.py -- run an MPW PowerPC tool (PEF: the old CodeWarrior MWCPPC / MWC68K) under Unicorn with a Python
StdCLib/InterfaceLib shim.
usage: python -I cwemu.py <tool.data> -- <tool args...>     (cwd = where paths resolve; Mac ':' paths map to '/')
The PEF loader is machost/mwppcemu_base.py's."""
import os, struct, sys, time
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "machost"))
import mwppcemu_base as M
from unicorn import Uc, UcError, UC_ARCH_PPC, UC_MODE_32, UC_MODE_BIG_ENDIAN, UC_HOOK_CODE, UC_HOOK_MEM_INVALID
from unicorn.ppc_const import UC_PPC_REG_LR, UC_PPC_REG_PC, UC_PPC_REG_MSR, UC_PPC_REG_FPR0

R = lambda n: M.REG(n)
STUB, STUBSZ = 0x0f000000, 0x20000
BASE = 0x10000000
RAM = 0x20000000
RET = STUB + STUBSZ - 4


class Exit(Exception):
    def __init__(self, code):
        self.code = code


def u32(v):
    return v & 0xffffffff


def s32(v):
    v &= 0xffffffff
    return v - (1 << 32) if v & 0x80000000 else v


def hp(p):  # mac path -> host path
    p = p.replace(":", "/")
    return M.case_blind(p)   # the Mac's file names are case-blind


class Emu:
    def __init__(self, path, args):
        self.args = args
        b = open(path, "rb").read()
        self.pef = M.Pef(b)
        rp = path[:-5] + ".rsrc" if path.endswith(".data") else path + ".rsrc"
        self.rsrc = open(rp, "rb").read() if os.path.exists(rp) else b""
        self.uc = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
        self.uc.mem_map(STUB, STUBSZ)
        self.uc.mem_map(0, 0x10000)  # Mac low memory: CW7's MWC68K reads through NULL, which yields 0 on a real Mac
        self.uc.mem_map(BASE, RAM)
        self.fds = {}
        self.nextfd = 3
        self.files = {}      # FILE* -> pyfile
        self.out = {1: sys.stdout.buffer, 2: sys.stderr.buffer}
        self.sizes = {}
        self.failure = None
        self.exitcode = None
        self.load()

    # ---- memory
    def rd(self, a, n):
        return bytes(self.uc.mem_read(a, n))

    def wr(self, a, d):
        self.uc.mem_write(a, bytes(d))

    def r32(self, a):
        return struct.unpack(">I", self.rd(a, 4))[0]

    def w32(self, a, v):
        self.wr(a, struct.pack(">I", v & 0xffffffff))

    def r16(self, a):
        return struct.unpack(">H", self.rd(a, 2))[0]

    def w16(self, a, v):
        self.wr(a, struct.pack(">H", v & 0xffff))

    def cstr(self, a):
        out = bytearray()
        while True:
            c = self.rd(a, 1)
            if c == b"\0":
                break
            out += c
            a += 1
        return bytes(out)

    def pstr(self, a):
        n = self.rd(a, 1)[0]
        return self.rd(a + 1, n)

    def alloc(self, n):
        n = (max(n, 8) + 15) & ~15
        p = self.heap
        self.heap += n
        if self.heap >= self.stack_low:
            raise RuntimeError("heap exhausted")
        self.sizes[p] = n
        return p

    # ---- loading
    def load(self):
        p, uc = self.pef, self.uc
        bases, images, cur = {}, {}, BASE
        for i, s in enumerate(p.secs):
            if s["kind"] in (0, 1, 2, 3):
                images[i] = p.image(s)
                bases[i] = cur
                cur = (cur + len(images[i]) + 0xfff) & ~0xfff
        self.heap = cur
        self.stack_top = BASE + RAM - 0x10000
        self.stack_low = self.stack_top - 0x1000000
        # imports with classes
        l = p.loader
        base = 56 + 24 * p.nlib
        names, classes = [], []
        for j in range(p.nimp):
            w = struct.unpack_from(">I", l, base + 4 * j)[0]
            names.append(p.cstr(w & 0xffffff))
            classes.append((w >> 24) & 0xf)
        self.names = names
        tv = STUB + 0x10000
        imp = []
        self.data_addr = {}
        for i, n in enumerate(names):
            if classes[i] == 1:   # data
                a = self.alloc(self.data_size(n))
                self.data_addr[n] = a
                imp.append(a)
            else:
                uc.mem_write(STUB + 4 * i, struct.pack(">I", 0x4e800020))
                uc.mem_write(tv + 8 * i, struct.pack(">II", STUB + 4 * i, 0))
                imp.append(tv + 8 * i)
        uc.mem_write(RET, struct.pack(">I", 0x4e800020))
        p.relocate(images, bases, imp)
        for i, img in images.items():
            uc.mem_write(bases[i], bytes(img))
        self.bases = bases
        uc.hook_add(UC_HOOK_CODE, self.on_import, begin=STUB, end=STUB + 4 * len(names))
        uc.hook_add(UC_HOOK_MEM_INVALID, self.on_bad)
        uc.reg_write(UC_PPC_REG_MSR, uc.reg_read(UC_PPC_REG_MSR) | 0x2000)
        self.init_data()

    def data_size(self, n):
        return {"_iob": 24 * 12, "_IntEnv": 128, "__p_CType": 4, "errno": 4}.get(n, 16)

    def init_data(self):
        D = self.data_addr
        # ctype table
        t = bytearray(256)
        for c in range(256):
            ch = chr(c)
            v = 0
            if "A" <= ch <= "Z":
                v |= 1
            if "a" <= ch <= "z":
                v |= 2
            if "0" <= ch <= "9":
                v |= 4
            if c in (9, 10, 11, 12, 13, 32):
                v |= 8
            if c < 128 and ch.isprintable() and not ch.isalnum() and ch != " ":
                v |= 0x10
            if c < 32 or c == 127:
                v |= 0x20
            if c == 32:
                v |= 0x40
            if ch in "0123456789abcdefABCDEF":
                v |= 0x80
            t[c] = v
        tp = self.alloc(256)
        self.wr(tp, t)
        if "__p_CType" in D:
            self.w32(D["__p_CType"], tp)
        # argv / IntEnv: {short; long argc@2; char** argv@6; char** envp@10; ...} (68k alignment)
        argv = [b"MWCPPC"] + [a.encode("mac_roman") for a in self.args]
        ptrs = []
        for a in argv:
            q = self.alloc(len(a) + 1)
            self.wr(q, a + b"\0")
            ptrs.append(q)
        av = self.alloc(4 * (len(ptrs) + 1))
        for i, q in enumerate(ptrs):
            self.w32(av + 4 * i, q)
        ev = self.alloc(8)
        ie = D["_IntEnv"]
        self.wr(ie, bytes(128))
        self.w32(ie + 2, len(argv))
        self.w32(ie + 6, av)
        self.w32(ie + 10, ev)
        self.iob = D["_iob"]
        for fd in range(3):
            self.files[self.iob + 24 * fd] = fd

    def on_bad(self, uc, access, addr, size, value, data):
        pc = uc.reg_read(UC_PPC_REG_PC)
        self.failure = "bad mem 0x%08x pc 0x%08x (code+0x%x) lr 0x%x" % (addr, pc, pc - BASE, uc.reg_read(UC_PPC_REG_LR) - BASE)
        return False

    def on_import(self, uc, addr, size, data):
        i = (addr - STUB) // 4
        name = self.names[i]
        fn = getattr(self, "f_" + name, None)
        args = [uc.reg_read(R(3 + k)) for k in range(8)]
        try:
            if fn is None:
                raise RuntimeError("import %s not implemented args=%s lr=0x%x" % (name, [hex(a) for a in args[:4]], uc.reg_read(UC_PPC_REG_LR) - BASE))
            r = fn(*args)
            if r is not None:
                uc.reg_write(R(3), u32(r))
        except Exit as e:
            self.exitcode = e.code
            uc.emu_stop()
        except Exception as e:
            self.failure = "%s: %s" % (name, e)
            uc.emu_stop()

    def run(self):
        uc, p = self.uc, self.pef
        tv = self.bases[p.main_s] + p.main_o
        code, toc = self.r32(tv), self.r32(tv + 4)
        uc.reg_write(R(1), self.stack_top - 0x100)
        uc.reg_write(R(2), toc)
        uc.reg_write(UC_PPC_REG_LR, RET)
        try:
            uc.emu_start(code, RET)
        except UcError as e:
            if self.exitcode is None:
                raise RuntimeError("%s; %s; pc=0x%x(code+0x%x)" % (e, self.failure, uc.reg_read(UC_PPC_REG_PC), uc.reg_read(UC_PPC_REG_PC) - BASE))
        if self.failure:
            raise RuntimeError(self.failure)
        if self.exitcode is None:
            self.exitcode = s32(uc.reg_read(R(3)))
        return self.exitcode

    # ---- varargs
    def fmt(self, f, nextint, nextdbl):
        f = f.decode("latin1")
        out = ""
        i = 0
        while i < len(f):
            c = f[i]
            if c != "%":
                out += c
                i += 1
                continue
            j = i + 1
            while j < len(f) and f[j] in "-+ #0":
                j += 1
            if f[j:j + 1] == "*":
                spec_w = str(s32(nextint())); j += 1
            else:
                k = j
                while f[j:j + 1].isdigit():
                    j += 1
                spec_w = f[k:j]
            prec = ""
            if f[j:j + 1] == ".":
                j += 1
                if f[j:j + 1] == "*":
                    prec = "." + str(s32(nextint())); j += 1
                else:
                    k = j
                    while f[j:j + 1].isdigit():
                        j += 1
                    prec = "." + f[k:j]
            flags = f[i + 1:j]
            lm = ""
            while f[j:j + 1] in ("l", "h", "L") and j < len(f):
                lm += f[j]
                j += 1
            cv = f[j:j + 1]
            j += 1
            pre = "%" + "".join(ch for ch in f[i + 1:] [: (len(f[i + 1:j]) )] if False)
            spec = "%" + "".join(ch for ch in flags if ch in "-+ #0") + spec_w + prec
            if cv in "di":
                v = s32(nextint())
                if lm == "h":
                    v = (v & 0xffff) - 0x10000 if v & 0x8000 else v & 0xffff
                out += (spec + "d") % v
            elif cv in "uxXo":
                v = u32(nextint())
                if lm == "h":
                    v &= 0xffff
                out += (spec + cv) % v
            elif cv == "c":
                out += (spec + "c") % chr(nextint() & 0xff)
            elif cv == "s":
                a = nextint()
                out += (spec + "s") % (self.cstr(a).decode("mac_roman") if a else "(null)")
            elif cv == "p":
                out += "%08x" % u32(nextint())
            elif cv in "feEgG":
                out += (spec + cv) % nextdbl()
            elif cv == "%":
                out += "%"
            elif cv == "n":
                nextint()
            else:
                out += f[i:j]
            i = j
        return out.encode("mac_roman", "replace")

    def varargs_regs(self, first_gpr):
        """returns (nextint, nextdbl) reading r{first_gpr}.. then the caller's parameter area."""
        uc = self.uc
        sp = uc.reg_read(R(1))   # we are in the callee entry: r1 is the caller's frame
        st = {"g": first_gpr, "f": 1, "s": sp + 0x18}

        def nextint():
            if st["g"] <= 10:
                v = uc.reg_read(R(st["g"]))
                st["g"] += 1
                return v
            v = self.r32(st["s"] + 4 * (st["g"] - 3))
            st["g"] += 1
            return v

        def nextdbl():
            if st["f"] <= 13:
                v = struct.unpack(">d", struct.pack(">Q", uc.reg_read(UC_PPC_REG_FPR0 + st["f"]) if False else self.fpr(st["f"])))[0]
                st["f"] += 1
                st["g"] += 2
                return v
            raise RuntimeError("double on stack")
        return nextint, nextdbl

    def fpr(self, n):
        v = self.uc.reg_read(UC_PPC_REG_FPR0 + n)
        if isinstance(v, tuple):
            v = v[0]
        return v if isinstance(v, int) else struct.unpack(">Q", struct.pack(">d", v))[0]

    def va_list(self, ap):
        st = {"p": ap}

        def nextint():
            v = self.r32(st["p"]); st["p"] += 4
            return v

        def nextdbl():
            v = struct.unpack(">d", self.rd(st["p"], 8))[0]; st["p"] += 8
            return v
        return nextint, nextdbl

    # ---- stdio plumbing
    def put(self, fp, data):
        f = self.files.get(fp)
        if f is None:
            raise RuntimeError("write to unknown FILE 0x%x" % fp)
        if isinstance(f, int):
            if f in self.out:
                self.out[f].write(data)
                self.out[f].flush()
            else:
                self.fds[f].write(data)
        else:
            f.write(data)

    def f_printf(self, fmt, *a):
        ni, nd = self.varargs_regs(4)
        s = self.fmt(self.cstr(fmt), ni, nd)
        self.put(self.iob + 24, s)
        return len(s)

    def f_fprintf(self, fp, fmt, *a):
        ni, nd = self.varargs_regs(5)
        s = self.fmt(self.cstr(fmt), ni, nd)
        self.put(fp, s)
        return len(s)

    def f_sprintf(self, buf, fmt, *a):
        ni, nd = self.varargs_regs(5)
        s = self.fmt(self.cstr(fmt), ni, nd)
        self.wr(buf, s + b"\0")
        return len(s)

    def f_vfprintf(self, fp, fmt, ap, *a):
        ni, nd = self.va_list(ap)
        s = self.fmt(self.cstr(fmt), ni, nd)
        self.put(fp, s)
        return len(s)

    def f_fputs(self, s, fp, *a):
        self.put(fp, self.cstr(s))
        return 0

    def f_fputc(self, c, fp, *a):
        self.put(fp, bytes([c & 0xff]))
        return c & 0xff

    def f__flsbuf(self, c, fp, *a):
        self.put(fp, bytes([c & 0xff]))
        # keep _cnt at 0 so the macro always calls back
        self.w32(fp, 0)
        return c & 0xff

    def f_fwrite(self, p, size, n, fp, *a):
        self.put(fp, self.rd(p, size * n))
        return n

    def f_fflush(self, fp, *a):
        return 0

    def f_perror(self, s, *a):
        sys.stderr.write("perror: %s\n" % (self.cstr(s).decode("latin1") if s else ""))

    # files
    def newfd(self, f):
        fd = self.nextfd
        self.nextfd += 1
        self.fds[fd] = f
        return fd

    def f_open(self, path, flags, *a):
        p = hp(self.cstr(path).decode("mac_roman"))
        # MPW: O_RDONLY 0, O_WRONLY 1, O_RDWR 2, O_APPEND 8, O_RSRC 16, O_ALIAS 32, O_CREAT 0x100, O_TRUNC 0x200, O_EXCL 0x400
        if flags & 16:
            return u32(-1)
        mode = {0: "rb", 1: "wb", 2: "r+b"}[flags & 3]
        try:
            if (flags & 3) != 0 and not (flags & 0x200) and os.path.exists(p):
                mode = "r+b"
            if (flags & 3) == 0 and not os.path.exists(p):
                return u32(-1)
            if flags & 0x100 and not os.path.exists(p):
                open(p, "wb").close()
            f = open(p, mode)
        except OSError as e:
            self.w32(self.data_addr["errno"], 2)
            return u32(-1)
        return self.newfd(f)

    def f_close(self, fd, *a):
        f = self.fds.pop(fd, None)
        if f:
            f.close()
        return 0

    def f_read(self, fd, buf, n, *a):
        f = self.fds.get(fd)
        if fd == 0:
            return 0
        d = f.read(n)
        self.wr(buf, d)
        return len(d)

    def f_write(self, fd, buf, n, *a):
        d = self.rd(buf, n)
        if fd in self.out:
            self.out[fd].write(d); self.out[fd].flush()
        else:
            self.fds[fd].write(d)
        return n

    def f_lseek(self, fd, off, whence, *a):
        f = self.fds[fd]
        f.seek(s32(off), whence)
        return f.tell()

    def f_faccess(self, path, cmd, *a):
        p = hp(self.cstr(path).decode("mac_roman"))
        return 0 if os.path.exists(p) else u32(-1)

    def f_remove(self, path, *a):
        try:
            os.remove(hp(self.cstr(path).decode("mac_roman")))
            return 0
        except OSError:
            return u32(-1)

    def f_fopen(self, path, mode, *a):
        p = hp(self.cstr(path).decode("mac_roman"))
        m = self.cstr(mode).decode().replace("b", "") + "b"
        try:
            f = open(p, m)
        except OSError:
            return 0
        for k in range(3, 12):
            fp = self.iob + 24 * k
            if fp not in self.files:
                self.files[fp] = f
                self.wr(fp, bytes(24))
                return fp
        return 0

    def f_fclose(self, fp, *a):
        f = self.files.pop(fp, None)
        if f is not None and not isinstance(f, int):
            f.close()
        return 0

    def f_fgets(self, buf, n, fp, *a):
        f = self.files[fp]
        if isinstance(f, int):
            return 0
        line = f.readline(n - 1)
        if not line:
            return 0
        self.wr(buf, line + b"\0")
        return buf

    def f_ftell(self, fp, *a):
        return self.files[fp].tell()

    def f_fseek(self, fp, off, wh, *a):
        self.files[fp].seek(s32(off), wh)
        return 0

    # memory/strings
    def f_malloc(self, n, *a):
        return self.alloc(n)

    def f_calloc(self, n, m, *a):
        return self.alloc(n * m)

    def f_free(self, p, *a):
        return None

    def f_realloc(self, p, n, *a):
        q = self.alloc(n)
        if p:
            old = self.sizes.get(p, 0)
            self.wr(q, self.rd(p, min(old, n)))
        return q

    def f_memcpy(self, d, s, n, *a):
        if n:
            self.wr(d, self.rd(s, n))
        return d

    f_memmove = f_memcpy

    def f_memset(self, d, c, n, *a):
        if n:
            self.wr(d, bytes([c & 0xff]) * n)
        return d

    def f_memcmp(self, a, b, n, *x):
        A, B = self.rd(a, n), self.rd(b, n)
        return (A > B) - (A < B)

    def f_memchr(self, p, c, n, *a):
        i = self.rd(p, n).find(bytes([c & 0xff]))
        return p + i if i >= 0 else 0

    def f_strlen(self, p, *a):
        return len(self.cstr(p))

    def f_strcpy(self, d, s, *a):
        self.wr(d, self.cstr(s) + b"\0")
        return d

    def f_strncpy(self, d, s, n, *a):
        x = self.cstr(s)[:n]
        self.wr(d, x + b"\0" * (n - len(x)))
        return d

    def f_strcat(self, d, s, *a):
        self.wr(d + len(self.cstr(d)), self.cstr(s) + b"\0")
        return d

    def f_strcmp(self, a, b, *x):
        A, B = self.cstr(a), self.cstr(b)
        return (A > B) - (A < B)

    def f_strncmp(self, a, b, n, *x):
        A, B = self.cstr(a)[:n], self.cstr(b)[:n]
        return (A > B) - (A < B)

    def f_strchr(self, p, c, *a):
        s = self.cstr(p) + b"\0"
        i = s.find(bytes([c & 0xff]))
        return p + i if i >= 0 else 0

    def f_strrchr(self, p, c, *a):
        s = self.cstr(p) + b"\0"
        i = s.rfind(bytes([c & 0xff]))
        return p + i if i >= 0 else 0

    def f_strspn(self, p, q, *a):
        s, t = self.cstr(p), set(self.cstr(q))
        n = 0
        while n < len(s) and s[n] in t:
            n += 1
        return n

    def f_toupper(self, c, *a):
        return c - 32 if 97 <= c <= 122 else c

    def f_tolower(self, c, *a):
        return c + 32 if 65 <= c <= 90 else c

    def f_abs(self, v, *a):
        return abs(s32(v))

    def f_atoi(self, p, *a):
        try:
            return int(self.cstr(p).split()[0])
        except Exception:
            return 0

    def f_strtol(self, p, endp, base, *a):
        s = self.cstr(p)
        i = 0
        while i < len(s) and s[i:i + 1].isspace():
            i += 1
        j = i
        if s[j:j + 1] in (b"-", b"+"):
            j += 1
        b = s32(base) or 10
        if b in (0, 16) and s[j:j + 2].lower() == b"0x":
            j += 2
            b = 16
        elif s32(base) == 0 and s[j:j + 1] == b"0":
            b = 8
        k = j
        while k < len(s) and s[k:k + 1].isalnum() and int(s[k:k + 1], 36) < b:
            k += 1
        v = int(s[j:k] or b"0", b) * (-1 if s[i:i + 1] == b"-" else 1)
        if endp:
            self.w32(endp, p + (k if k > j else 0))
        return v
    # ---- MathLib (lane EVCC): float literal conversion in MWCPPC 1.x
    def f_feclearexcept(self, *a):
        return 0

    def f_fetestexcept(self, *a):
        return 0

    def f_dec2num(self, p, *a):
        raw = bytes(self.uc.mem_read(p, 42))
        sgn, exp = raw[0], struct.unpack_from(">h", raw, 2)[0]
        digits = raw[5:5 + raw[4]].decode("latin1")
        if digits[:1] in ("I", "i"):
            v = float("inf")
        elif digits[:1] in ("N", "n"):
            v = float("nan")
        else:
            v = float("%se%d" % (digits or "0", exp))
        if sgn:
            v = -v
        self.uc.reg_write(UC_PPC_REG_FPR0 + 1, struct.unpack(">Q", struct.pack(">d", v))[0])
        return None

    def f_atof(self, p, *a):
        raise RuntimeError("atof")

    def f_exit(self, code, *a):
        raise Exit(s32(code))

    def f_atexit(self, f, *a):
        return 0

    def f_signal(self, *a):
        return 0

    def f___setjmp(self, buf, *a):
        return 0

    def f_getenv(self, name, *a):
        k = self.cstr(name).decode('latin1')
        v = os.environ.get('MPW_' + k)
        if v is None: return 0
        if not hasattr(self, '_env'): self._env = {}
        if k not in self._env:
            q = self.alloc(len(v) + 2); self.wr(q, v.encode('mac_roman') + bytes(1)); self._env[k] = q
        return self._env[k]
    def f_getenv_old(self, *a):
        return 0

    def f__BreakPoint(self, *a):
        raise RuntimeError("BreakPoint")

    def f_time(self, p, *a):
        t = int(time.time()) + 2082844800
        if p:
            self.w32(p, t)
        return t

    # ---- InterfaceLib (just what MrC touched)
    def f_NewHandle(self, n, *a):
        return self.newh(n)

    f_NewHandleClear = f_NewHandle
    f_TempNewHandle = lambda self, n, perr, *a: (self.w16(perr, 0), self.newh(n))[1]

    def newh(self, n):
        h = self.alloc(4)
        self.w32(h, self.alloc(n))
        self.sizes[("h", h)] = n
        return h

    def f_SetHandleSize(self, h, n, *a):
        old = self.sizes.get(("h", h), 0)
        q = self.alloc(n)
        self.wr(q, self.rd(self.r32(h), min(old, n)))
        self.w32(h, q)
        self.sizes[("h", h)] = n

    def f_GetHandleSize(self, h, *a):
        return self.sizes.get(("h", h), 0)

    def f_DisposePtr(self, *a):
        return None

    f_TempDisposeHandle = f_DisposePtr
    f_HUnlock = f_DisposePtr
    f_HLockHi = f_DisposePtr
    f_TempHLock = f_DisposePtr
    f_ReleaseResource = f_DisposePtr
    f_SetResLoad = f_DisposePtr
    f_ShowCursor = f_DisposePtr
    f_Debugger = f_DisposePtr

    def f_HGetState(self, h, *a):
        return 0

    f_HSetState = f_DisposePtr

    def f_FreeMem(self, *a):
        return 0x4000000

    f_PurgeSpace = lambda self, tot, cont, *a: (self.w32(tot, 0x4000000), self.w32(cont, 0x4000000))[0]
    f_TempMaxMem = lambda self, grow, *a: (self.w32(grow, 0), 0x4000000)[1]
    f_StackSpace = lambda self, *a: self.uc.reg_read(R(1)) - self.stack_low
    f_GetApplLimit = lambda self, *a: self.stack_low
    f_TickCount = lambda self, *a: int(time.time() * 60) & 0xffffffff
    f_LMGetCurrentA5 = lambda self, *a: 0
    f_ResError = lambda self, *a: 0

    def f_BlockMove(self, s, d, n, *a):
        if n:
            self.wr(d, self.rd(s, n))
        return 0

    def f_c2pstr(self, p, *a):
        s = self.cstr(p)
        self.wr(p, bytes([len(s) & 0xff]) + s)
        return p

    # resources: parse the .rsrc fork
    def res_map(self):
        if hasattr(self, "_rm"):
            return self._rm
        d = self.rsrc
        rm = {}
        if d:
            dofs, mofs, dlen, mlen = struct.unpack_from(">IIII", d, 0)
            tl = mofs + struct.unpack_from(">H", d, mofs + 24)[0]
            nl = mofs + struct.unpack_from(">H", d, mofs + 26)[0]
            nt = struct.unpack_from(">H", d, tl)[0] + 1
            for t in range(nt):
                typ, cnt, ro = struct.unpack_from(">4sHH", d, tl + 2 + 8 * t)
                for r in range(cnt + 1):
                    rid, no, a_hi, a_lo, _h = struct.unpack_from(">hHBHxxxx", d, tl + ro + 12 * r) if False else (None,) * 5
                    rid = struct.unpack_from(">h", d, tl + ro + 12 * r)[0]
                    off3 = d[tl + ro + 12 * r + 5: tl + ro + 12 * r + 8]
                    off = (off3[0] << 16) | (off3[1] << 8) | off3[2]
                    ln = struct.unpack_from(">I", d, dofs + off)[0]
                    rm[(typ, rid)] = d[dofs + off + 4: dofs + off + 4 + ln]
        self._rm = rm
        return rm

    def f_Get1Resource(self, typ, rid, *a):
        return self.f_GetResource(typ, rid)

    def f_HomeResFile(self, h, *a):
        return 1

    def f_GetResource(self, typ, rid, *a):
        t = struct.pack(">I", typ)
        r = self.res_map().get((t, struct.unpack(">h", struct.pack(">H", rid & 0xffff))[0]))
        if r is None:
            return 0
        h = self.newh(len(r))
        self.wr(self.r32(h), r)
        return h

    def f_GetIndString(self, buf, rid, idx, *a):
        r = self.res_map().get((b"STR#", struct.unpack(">h", struct.pack(">H", rid & 0xffff))[0]))
        if r is None:
            self.wr(buf, b"\0")
            return
        n = struct.unpack_from(">H", r, 0)[0]
        o = 2
        for i in range(1, n + 1):
            ln = r[o]
            if i == idx:
                self.wr(buf, r[o:o + 1 + ln])
                return
            o += 1 + ln
        self.wr(buf, b"\0")

    def f_Microseconds(self, p, *a):
        t = int(time.time() * 1e6)
        self.w32(p, t >> 32)
        self.w32(p + 4, t & 0xffffffff)

    def f_NewRoutineDescriptor(self, proc, info, isa, *a):   # MWC68K (CW8+): native code, the proc itself
        return proc

    f_DisposeRoutineDescriptor = lambda self, *a: 0

    def f_CallUniversalProc(self, proc, info, *a):
        # MWC68K's `_68K` code resources (SANE glue for float constants): 68k code, the "descriptor" above is the
        # resource itself (lane EV68CC)
        if not hasattr(self, "mm68"):
            sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
            import mm68k
            self.mm68 = mm68k.MM68(self.uc)
        return self.mm68.call(proc, info, a)

    def f_NewPtr(self, n, *a):
        return self.alloc(n)

    f_HLock = f_DisposePtr
    f_DisposeHandle = f_DisposePtr
    f_TempHUnlock = f_DisposePtr
    f_MemError = lambda self, *a: 0
    f_CurResFile = lambda self, *a: 1
    f_MaxMem = lambda self, grow, *a: (self.w32(grow, 0), 0x4000000)[1]

    def f_ctime(self, p, *a):
        q = self.alloc(32)
        self.wr(q, time.strftime("%a %b %d %H:%M:%S %Y").encode() + bytes([32,0]))
        return q

    def f_localtime(self, p, *a):
        q = self.alloc(48)
        t = time.localtime()
        for i, v in enumerate((t.tm_sec, t.tm_min, t.tm_hour, t.tm_mday, t.tm_mon - 1, t.tm_year - 1900, (t.tm_wday + 1) % 7, t.tm_yday - 1, 0)):
            self.w32(q + 4 * i, v)
        return q

    def f_ResolvePath(self, raw, res, isf, had, *a):
        p = self.cstr(raw)
        self.wr(res, p + bytes([0]))
        ex = os.path.exists(hp(p.decode("mac_roman")))
        if isf:
            self.wr(isf, bytes([1 if os.path.isdir(hp(p.decode("mac_roman"))) else 0]))
        if had:
            self.wr(had, bytes([0]))
        return 0 if ex else u32(-43)


    def pbname(self, pb):
        nm = self.r32(pb + 18)
        return self.pstr(nm).decode("mac_roman") if nm else ""

    def f_PBOpenSync(self, pb, *a):
        p = hp(self.pbname(pb))
        if not os.path.isfile(p):
            self.w16(pb + 16, 0xffd5)
            return u32(-43)
        fd = self.newfd(open(p, "rb"))
        self.w16(pb + 24, fd)
        self.w16(pb + 16, 0)
        return 0

    def f_PBReadSync(self, pb, *a):
        fd = self.r16(pb + 24)
        buf, cnt, mode, off = self.r32(pb + 32), self.r32(pb + 36), self.r16(pb + 44), self.r32(pb + 46)
        f = self.fds[fd]
        if mode & 3 == 1:
            f.seek(off)
        elif mode & 3 == 3:
            f.seek(s32(off), 1)
        d = f.read(cnt)
        self.wr(buf, d)
        self.w32(pb + 40, len(d))
        err = 0 if len(d) == cnt else u32(-39)
        self.w16(pb + 16, err & 0xffff)
        return err

    def f_PBCloseSync(self, pb, *a):
        f = self.fds.pop(self.r16(pb + 24), None)
        if f:
            f.close()
        self.w16(pb + 16, 0)
        return 0

    def f_PBGetEOFAsync(self, pb, *a):
        f = self.fds[self.r16(pb + 24)]
        cur = f.tell()
        f.seek(0, 2)
        self.w32(pb + 28, f.tell())
        f.seek(cur)
        self.w16(pb + 16, 0)
        return 0

    def f_PBGetFPosSync(self, pb, *a):
        f = self.fds[self.r16(pb + 24)]
        self.w32(pb + 46, f.tell())
        self.w16(pb + 16, 0)
        return 0

    def spec_new(self, spec, path):
        if not hasattr(self, "specs"):
            self.specs = {}
        i = len(self.specs) + 1
        self.specs[i] = path
        base = os.path.basename(path.rstrip("/")).encode("mac_roman")[:63]
        self.w16(spec, 0xffff)
        self.w32(spec + 2, i)
        self.wr(spec + 6, bytes([len(base)]) + base + bytes(63 - len(base)))


    def f_MakeResolvedFSSpec(self, vol, dirid, path, spec, isf, had, leaf, *a):
        p = hp(self.pstr(path).decode("mac_roman"))
        self.spec_new(spec, p)
        if isf:
            self.wr(isf, bytes([1 if os.path.isdir(p) else 0]))
        if had:
            self.wr(had, bytes([0]))
        if leaf:
            self.wr(leaf, bytes([0]))
        return 0 if os.path.exists(p) else u32(-43)



    def f_Create(self, name, vref, creator, ftype, *a):
        p = hp(self.pstr(name).decode("mac_roman"))
        if os.path.exists(p):
            return u32(-48)
        open(p, "wb").close()
        return 0


    def f_FSDelete(self, name, vref, *a):
        try:
            os.remove(hp(self.pstr(name).decode("mac_roman")))
            return 0
        except OSError:
            return u32(-43)


    def f_PBSetCatInfoSync(self, pb, *a):
        self.w16(pb + 16, 0)
        return 0

    def guest(self, tv, *args):
        uc = self.uc
        ctx = uc.context_save()
        sp = uc.reg_read(R(1))
        code, toc = self.r32(tv), self.r32(tv + 4)
        uc.reg_write(R(1), (sp - 0x400) & ~0xf)
        uc.reg_write(R(2), toc)
        for k, v in enumerate(args):
            uc.reg_write(R(3 + k), v)
        uc.reg_write(UC_PPC_REG_LR, RET)
        uc.emu_start(code, RET)
        r = uc.reg_read(R(3))
        uc.context_restore(ctx)
        return s32(r)

    def f_qsort(self, base, n, size, cmp, *a):
        import functools
        items = [self.rd(base + i * size, size) for i in range(n)]
        tmp1, tmp2 = self.alloc(size), self.alloc(size)

        def c(x, y):
            self.wr(tmp1, x)
            self.wr(tmp2, y)
            return self.guest(cmp, tmp1, tmp2)
        items.sort(key=functools.cmp_to_key(c))
        for i, it in enumerate(items):
            self.wr(base + i * size, it)

    def f_bsearch(self, key, base, n, size, cmp, *a):
        lo, hi = 0, n
        while lo < hi:
            mid = (lo + hi) // 2
            r = self.guest(cmp, key, base + mid * size)
            if r == 0:
                return base + mid * size
            if r < 0:
                hi = mid
            else:
                lo = mid + 1
        return 0

    def f_GetToolTrapAddress(self, *a):
        return 0

    def f_setvbuf(self, *a):
        return 0
    def f_InitGraf(self, *a): return 0
    def f_SetCursor(self, *a): return 0
    def f_GetCursor(self, *a): return 0
    def f_PtrAndHand(self, *a): return 0
    def f_TrapAvailable(self, *a): return 1
    def f_Gestalt(self, *a): return u32(-5551)

    def f_GetDateTime(self, p, *a):
        self.w32(p, int(time.time()) + 2082844800)
    f_LMGetTime = lambda self, *a: int(time.time()) + 2082844800
    f_LMGetTicks = lambda self, *a: int(time.time() * 60) & 0xffffffff
    f_LMGetMemErr = lambda self, *a: 0
    f_MoveHHi = f_DisposePtr
    f_NewPtrClear = f_NewPtr
    f_FindFolder = lambda self, *a: u32(-43)
    f_IUMagIDString = lambda self, a, b, la, lb, *x: 0 if (la == lb and self.rd(a, la).lower() == self.rd(b, lb).lower()) else 1

    def f_p2cstr(self, p, *a):
        n = self.rd(p, 1)[0]
        self.wr(p, self.rd(p + 1, n) + bytes(1))
        return p
    def f_PLstrcpy(self, d, s, *a):
        n = self.rd(s, 1)[0]
        self.wr(d, self.rd(s, n + 1)); return d
    def f_PLstrlen(self, s, *a): return self.rd(s, 1)[0]
    def f_PLstrcat(self, d, s, *a):
        n = self.rd(d, 1)[0]; m = self.rd(s, 1)[0]
        self.wr(d + 1 + n, self.rd(s + 1, m)); self.wr(d, bytes([(n + m) & 255])); return d
    def f_PLpos(self, a, b, *x):
        la = self.rd(a, 1)[0]; lb = self.rd(b, 1)[0]
        return self.rd(b + 1, lb).find(self.rd(a + 1, la)) + 1
    def f_numtostring(self, *a): raise RuntimeError("numtostring")
    f_NumToString = f_numtostring

    def mp(self, name):
        return hp(self.pstr(name).decode("mac_roman"))
    def f_FSRead(self, ref, cnt, buf, *a):
        n = self.r32(cnt); d = self.fds[ref].read(n)
        self.wr(buf, d); self.w32(cnt, len(d))
        return 0 if len(d) == n else u32(-39)
    def f_FSWrite(self, ref, cnt, buf, *a):
        n = self.r32(cnt); self.fds[ref].write(self.rd(buf, n)); return 0
    def f_FSClose(self, ref, *a):
        f = self.fds.pop(ref, None)
        if f: f.close()
        return 0
    def f_GetEOF(self, ref, p, *a):
        f = self.fds[ref]; c = f.tell(); f.seek(0, 2); self.w32(p, f.tell()); f.seek(c); return 0
    def f_SetEOF(self, ref, n, *a):
        f = self.fds[ref]; f.truncate(n); return 0
    def f_GetFPos(self, ref, p, *a):
        self.w32(p, self.fds[ref].tell()); return 0
    def f_SetFPos(self, ref, mode, off, *a):
        f = self.fds[ref]; off = s32(off)
        mode &= 3
        if mode == 1: f.seek(off)
        elif mode == 2: f.seek(off, 2)
        elif mode == 3: f.seek(off, 1)
        return 0
    def f_FSpSetFInfo(self, *a): return 0
    def f_ResolveAliasFile(self, spec, chain, isf, was, *a):
        if isf: self.wr(isf, bytes(1))
        if was: self.wr(was, bytes(1))
        return 0
    def f_GetVInfo(self, *a): return u32(-35)

    # ---- directory-id model: id -> absolute host dir
    def dpath(self, did):
        if not hasattr(self, "dirs"): self.dirs = {}
        return self.dirs.get(did, os.getcwd())
    def dirid(self, path):
        if not hasattr(self, "dirs"): self.dirs = {}
        path = os.path.abspath(path)
        if path == os.path.dirname(path):
            self.dirs[2] = path; return 2
        for k, v in self.dirs.items():
            if v == path: return k
        k = 100 + len(self.dirs)
        self.dirs[k] = path
        return k
    def full(self, did, name):
        n = hp(name.decode("mac_roman") if isinstance(name, bytes) else name)
        if n.startswith("/"): n = n[1:]
        return os.path.normpath(os.path.join(self.dpath(did), n))
    def mpd(self, did, name):
        return self.full(did, self.pstr(name))
    def f_FSMakeFSSpec(self, vol, did, name, spec, *a):
        p = self.mpd(did, name)
        par = os.path.dirname(p)
        self.w16(spec, 0xffff); self.w32(spec + 2, self.dirid(par))
        base = os.path.basename(p).encode("mac_roman")[:63]
        self.wr(spec + 6, bytes([len(base)]) + base + bytes(63 - len(base)))
        return 0 if os.path.exists(p) else u32(-43)
    def f_HOpen(self, vol, did, name, perm, refp, *a):
        p = self.mpd(did, name)
        if not os.path.isfile(p): return u32(-43)
        f = open(p, "rb" if perm & 3 == 1 else "r+b")
        self.w16(refp, self.newfd(f)); return 0
    def f_HCreate(self, vol, did, name, creator, ftype, *a):
        p = self.mpd(did, name)
        if os.path.exists(p): return u32(-48)
        open(p, "wb").close(); return 0
    def f_HDelete(self, vol, did, name, *a):
        try: os.remove(self.mpd(did, name)); return 0
        except OSError: return u32(-43)
    def f_HGetFInfo(self, vol, did, name, fi, *a):
        p = self.mpd(did, name)
        if not os.path.exists(p): return u32(-43)
        self.wr(fi, b"TEXT" + b"MPS " + bytes(12)); return 0
    def f_PBHGetFInfoSync(self, pb, *a):
        p = self.full(self.r32(pb + 48), self.pstr(self.r32(pb + 18)))
        if not os.path.isfile(p):
            self.w16(pb + 16, 0xffd5); return u32(-43)
        self.wr(pb + 32, b"TEXT" + b"MPS " + bytes(12)); self.w16(pb + 16, 0); return 0
    def f_PBHOpenSync(self, pb, *a):
        p = self.full(self.r32(pb + 48), self.pstr(self.r32(pb + 18)))
        if not os.path.isfile(p):
            self.w16(pb + 16, 0xffd5); return u32(-43)
        self.w16(pb + 24, self.newfd(open(p, "rb"))); self.w16(pb + 16, 0); return 0
    def f_PBGetCatInfoSync(self, pb, *a):
        did = self.r32(pb + 48); nm = self.r32(pb + 18); idx = self.r16(pb + 28)
        if idx == 0xffff:
            p = self.dpath(did)
            if nm: self.wr(nm, bytes([min(len(os.path.basename(p)),31)]) + os.path.basename(p).encode("mac_roman")[:31])
            self.wr(pb + 30, bytes([0x10]))
            self.w32(pb + 48, did); self.w32(pb + 100, 1 if did == 2 else self.dirid(os.path.dirname(p)))
            if did == 2 and nm: self.wr(nm, b"Vol")
            self.w16(pb + 16, 0); return 0
        name = self.pstr(nm) if nm else b""
        p = self.full(did, name) if name else self.dpath(did)
        if not os.path.exists(p):
            self.w16(pb + 16, 0xffd5); return u32(-43)
        isdir = os.path.isdir(p)
        self.wr(pb + 30, bytes([0x10 if isdir else 0]))
        self.wr(pb + 32, b"TEXT" + b"MPS " + bytes(12))
        if isdir:
            self.w32(pb + 48, self.dirid(p)); self.w32(pb + 100, self.dirid(os.path.dirname(p)))
        else:
            self.w32(pb + 54, os.path.getsize(p)); self.w32(pb + 100, self.dirid(os.path.dirname(p)))
        self.w16(pb + 16, 0); return 0

    def spec_path(self, spec):
        return self.full(self.r32(spec + 2), self.pstr(spec + 6))
    def f_FSpCreate(self, spec, creator, ftype, script, *a):
        p = self.spec_path(spec)
        if os.path.exists(p): return u32(-48)
        open(p, "wb").close(); return 0
    def f_FSpOpenDF(self, spec, perm, refp, *a):
        p = self.spec_path(spec)
        if perm & 3 == 1:
            if not os.path.isfile(p): return u32(-43)
            f = open(p, "rb")
        else:
            f = open(p, "r+b" if os.path.exists(p) else "w+b")
        self.w16(refp, self.newfd(f)); return 0
    def f_FSpGetFInfo(self, spec, fi, *a):
        p = self.spec_path(spec)
        if not os.path.exists(p): return u32(-43)
        self.wr(fi, b"TEXT" + b"MPS " + bytes(12)); return 0

    f_HOpenDF = f_HOpen

if __name__ == "__main__":
    i = sys.argv.index("--")
    e = Emu(sys.argv[1], sys.argv[i + 1:])
    code = e.run()
    sys.exit(code if code >= 0 else 1)

"""Run an old CodeWarrior MPW tool compiler (MWCPPC / MWC68K 1.3 to 2.0: CW Gold 7, CW Pro 1, CW Pro 2).

The tool is a PEF (data fork <tool>.data + resource fork <tool>.rsrc, extracted from the CD image into
toolchain/oldcw/<dir>/) run under Unicorn with a StdCLib/InterfaceLib shim (cwemu.py).  The tool wants Mac
conventions, so a work directory build/mpwhost/<dir>[-68k]/<stem>/ holds CR-line-ending copies of the source, of
the project headers (sound.h renamed evsound.h: it would shadow <Sound.h>) and of the CD's CIncludes, all
reachable by simple relative names.

compile(compdir, src, obj, user, flags, shim, root, tool): returns (ok, message); flags in the tool's spelling
(`-opt l4`; the 1.x tool has no -proc/-sym/-enum).  System headers: the compiler's own CIncludes plus the shim."""
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


def cr(b):
    return b.replace(b"\r\n", b"\n").replace(b"\n", b"\r")


def sync(srcdir, dst, convert):
    os.makedirs(dst, exist_ok=True)
    if convert:     # user dirs: drop copies the source dir no longer has (a stale header shadows the real one)
        want = {("evsound.h" if f == "sound.h" else f) for f in os.listdir(srcdir)}
        for f in os.listdir(dst):
            if f not in want and os.path.isfile(os.path.join(dst, f)):
                os.remove(os.path.join(dst, f))
    for f in os.listdir(srcdir):
        a = os.path.join(srcdir, f)
        if not os.path.isfile(a):
            continue
        nm = "evsound.h" if (f == "sound.h" and convert) else f
        b = os.path.join(dst, nm)
        if convert:
            d = cr(open(a, "rb").read()).replace(b'"sound.h"', b'"evsound.h"')
            if not os.path.exists(b) or open(b, "rb").read() != d:
                open(b, "wb").write(d)
        elif not os.path.exists(b) or os.path.getmtime(a) > os.path.getmtime(b):
            shutil.copy(a, b)


def compile(compdir, src, obj, user, flags, shim, root, tool="MWCPPC"):
    ver = os.path.basename(os.path.normpath(compdir)) + ("" if tool == "MWCPPC" else "-68k")
    stem = os.path.splitext(os.path.basename(src))[0].replace(" ", "_")
    w = os.path.join(root, "build", "mpwhost", ver, stem)
    os.makedirs(w, exist_ok=True)
    si = ["sys", "shim"]
    sync(os.path.join(compdir, "CIncludes"), os.path.join(w, "sys"), False)
    sync(shim, os.path.join(w, "shim"), True)
    ui = []
    for i, u in enumerate(user):
        sync(u, os.path.join(w, "u%d" % i), True)
        ui.append("u%d" % i)
    tmp = os.path.join(w, os.path.basename(src))
    open(tmp, "wb").write(cr(open(src, "rb").read()))
    o = os.path.join(w, "o.o")
    if os.path.exists(o):
        os.remove(o)
    env = dict(os.environ)
    env["MPW_MWCIncludes"] = ",".join(si)
    cmd = [sys.executable, "-I", os.path.join(HERE, "cwemu.py"), os.path.join(compdir, tool + ".data"), "--", *flags,
           *[a for u in ui + si for a in ("-i", u)], os.path.basename(src), "-o", "o.o"]
    # a runaway emulated compile must not outlive the caller
    try:
        r = subprocess.run(cmd, capture_output=True, cwd=w, env=env, timeout=900)
    except subprocess.TimeoutExpired as e:
        return False, "mpwhost: compile timed out after %ss" % e.timeout
    msg = (r.stdout + r.stderr).decode("mac_roman").replace("\r", "\n")
    if not os.path.exists(o):
        return False, msg
    shutil.copy(o, obj)
    return True, msg

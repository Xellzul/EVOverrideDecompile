"""The Mac-hosted CW Pro 3 compiler plugins ("MW C/C++ PPC", "MW C/C++ 68K", the CD's data+resource forks in
toolchain/mwcppc/compilers/Pro30mac) run under Unicorn (mwppc3.py; the 68K plugin's SANE `_68K` code resources
run on Unicorn's m68k with the FP68K/DecStr68K traps modelled by sane.py).

compile(kind, src, obj, user, system, flags, root): kind "ppc" or "68k"; flags in MWCPPC.exe spelling.
The plugins' preference panels for a flag set come from build/machost/panels-<key>.txt, seeded from buildkit/panels/
(dumped once on Windows from the CW Pro 3 Windows CD's plugins; they depend on the flags only)."""
import hashlib
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PLUGIN = {"ppc": "MW_C_C++_PPC", "68k": "MW_C_C++_68K"}


def panels(kind, flags, root, env):
    key = hashlib.md5((kind + " ".join(flags) + env.get("HOST68K_PREFS", "")).encode()).hexdigest()[:12]
    out = os.path.join(root, "build", "machost", "panels-%s.txt" % key)
    if not os.path.exists(out):
        sys.exit("machost: no preference panels for this flag set (%s %s; HOST68K_PREFS %s): buildkit/panels/ "
                 "needs panels-%s.txt, dumped on Windows" % (kind, " ".join(flags), env.get("HOST68K_PREFS", "-"), key))
    return out


def compile(kind, src, obj, user, system, flags, root, env=None):
    env = dict(os.environ if env is None else env)
    pan = panels(kind, flags, root, env)
    plug = os.path.join(root, "toolchain", "mwcppc", "compilers", "Pro30mac", PLUGIN[kind])
    cmd = [sys.executable, "-I", os.path.join(HERE, "mwppc3.py"), "--plugin", plug, "--panels-from-dump", pan,
           "--sysdirs", ";".join(system)]
    for u in user:
        cmd += ["-i", u]
    if kind == "ppc":
        cmd.append("--ppc")
    if any(flags[i:i + 2] == ["-sym", "on"] for i in range(len(flags))):
        cmd.append("--sym")
    cmd += ["-o", obj, src]
    return subprocess.run(cmd, capture_output=True, text=True, env=env)

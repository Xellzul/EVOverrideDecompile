"""The 68k compile step: one source file to a CodeWarrior 68k object.

compile() runs the CW Pro 3 CD's Mac-hosted "MW C/C++ 68K" 2.1 b0013 (toolchain/mwcppc/compilers/Pro30mac) under
Unicorn (machost/), with PRESETS["evo"] and a 68K CodeGen panel (PRESET_PREFS); mpw68_compile() an old CodeWarrior MPW
MWC68K (a library part of buildkit/decomp.toml [link68] parts) under mpwhost/. Objects land in build/68k/b.
"""
import os
import shutil
import sys

J = os.path.join
TOOLS = os.path.dirname(os.path.abspath(__file__))                   # buildkit/tools
ROOT = os.path.dirname(os.path.dirname(TOOLS))                        # the repository
SCR = J(ROOT, "build", "68k")
BUILD = J(SCR, "b")
SHADOW = J(SCR, "sh")
INCDIR = J(ROOT, "sources", "include")
# Universal Interfaces 3.2 (CW Pro 5's): the game's 68k system headers, as on PPC
UI32 = J(ROOT, "toolchain", "cwpro5-win", "MacOS Support", "Universal", "Interfaces", "CIncludes")
# MSL headers of the game's 68k build: the CW Pro 3 CD's MSL C 2.4 (the compiler's own release). Pro 5's
# cmath.macos.h maps sin -> sind etc. on 68K unconditionally and links SANE "d" glue the target lacks;
# Pro 3's cmath maps them only under 8-byte doubles.
_P3 = "toolchain/cwpro3-cd/win/Lang_CCPP/Metrowerks_Standard_Library/MSL_C/"
MSL68 = [_P3 + "MSL_Common/Public_Includes", _P3 + "MSL_Mac/Public_Includes"]
PRESETS = {
    "evo": "-opt off,peephole -model smart,farData,farStrings,farvTables -intsize 2 -dsize 80 -mbg off -enum min -sym on -Cpp_exceptions off",
}
# 68K CodeGen panel bytes (HOST68K_PREFS) per preset: the flags cannot reach byte 6 (global register
# allocation) without also turning the IR optimizer on, so the panel is given whole.
# byte 3 = 68020 code (the target has rtd, divs.l, mulu.l), byte 9 off = near strings
PRESET_PREFS = {"evo": "0600010100010100010000000000000001000000010000000000000000000000"}


def shadow(src):
    """MWC68K searches the including file's directory for <...>: project sound.h would shadow
    <Sound.h>. Work on a copy with it renamed."""
    inc, game = J(SHADOW, "inc"), J(SHADOW, "game")
    for d in (inc, game):
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d)
    for f in os.listdir(INCDIR):
        if f.endswith(".h"):
            t = open(J(INCDIR, f), "rb").read()
            t = t.replace(b'"sound.h"', b'"evsound.h"')
            open(J(inc, "evsound.h" if f == "sound.h" else f), "wb").write(t)
    t = open(src, "rb").read().replace(b'"sound.h"', b'"evsound.h"')
    out = J(game, os.path.basename(src))
    open(out, "wb").write(t)
    return out


def compile(src, flags, prefs=None, ui=None):
    """The Mac-hosted 68K plugin under Unicorn (machost): exact SANE extended constants. PREFS: the 68K CodeGen panel
    (hex, HOST68K_PREFS); UI: the Universal Interfaces CIncludes (default UI32). Returns build/68k/b/<stem>.o."""
    sys.path.insert(0, J(TOOLS, "machost"))
    import machost
    s = shadow(src)
    system = [ui or UI32] + [J(ROOT, *d.split("/")) for d in MSL68]
    env = dict(os.environ, MWCIncludes=";".join(system))
    if prefs:
        env["HOST68K_PREFS"] = prefs
    os.makedirs(BUILD, exist_ok=True)
    obj = J(BUILD, os.path.splitext(os.path.basename(src))[0] + ".o")
    if os.path.exists(obj):
        os.remove(obj)
    r = machost.compile("68k", s, obj, [J(SHADOW, "inc"), J(SHADOW, "game")], system, flags, ROOT, env)
    if not os.path.exists(obj):
        sys.exit("COMPILE FAILED\n" + (r.stdout + r.stderr)[:3000])
    return obj


def mpw68_compile(compdir, src, obj, flags):
    """[link68] parts {stem = {compiler = "mpw68:<dir>"}}: an old CodeWarrior MPW MWC68K (a PPC-native MPW tool,
    <dir>/MWC68K.data + .rsrc, from the CW Gold CD images) under mpwhost/, with the tool's own CIncludes and the
    sources/include/cw8 shims. Returns (ok, messages)."""
    sys.path.insert(0, J(TOOLS, "mpwhost"))
    import mpwhost
    return mpwhost.compile(compdir, src, obj, [os.path.dirname(os.path.abspath(src)), INCDIR], flags,
                           J(ROOT, "sources", "include", "cw8"), ROOT, tool="MWC68K")

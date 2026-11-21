"""The PowerPC compile step: one source file to a CodeWarrior object.

buildkit/decomp.toml [build] names the project compiler and flags; a file's `// COMPILE:` line (first 30 lines)
replaces the flags and its `// COMPILER: <id>` (an id of [build.compilers]) the compiler of a separately built part (a
library). A function is marked by `// FUNCTION: EVO 0x1000xxxx` directly above its definition. A compiler is one of
  emu:<dir>   the CW Pro 3 Mac-hosted plugin under Unicorn (machost/)
  mpw:<dir>   a CodeWarrior Gold / Pro MPW tool MWCPPC under Unicorn (mpwhost/)
  <dir>       a Windows-hosted MWCPPC.exe
compile_object() writes build/<stem>.o.
"""
from __future__ import annotations

import os
import re
import subprocess
import sys
import tomllib

TOOLS = os.path.dirname(os.path.abspath(__file__))                   # buildkit/tools
KIT = os.path.dirname(TOOLS)                                          # buildkit/
ROOT = os.path.dirname(KIT)                                           # the repository: sources/, toolchain/, game/

MARKER = re.compile(r"^\s*//\s*FUNCTION:\s*EVO\s+(0x[0-9A-Fa-f]+)")
COMPILE = re.compile(r"^\s*//\s*COMPILE:\s*(.*)$")
COMPILER = re.compile(r"^\s*//\s*COMPILER:\s*(\S+)")
DEFN = re.compile(r"([A-Za-z_]\w*)\s*\(")


def load_config() -> dict:
    """buildkit/decomp.toml"""
    with open(os.path.join(KIT, "decomp.toml"), "rb") as fh:
        return tomllib.load(fh)


def read_source(path: str) -> tuple[list[str] | None, list[tuple[int, str]]]:
    """The file's `// COMPILE:` flags (or None) and its [(marker address, function name)]."""
    lines = open(path, encoding="mac-roman", errors="replace").read().splitlines()
    flags = None
    for line in lines[:30]:
        m = COMPILE.match(line)
        if m:
            flags = m.group(1).split()
            break
    funcs = []
    for i, line in enumerate(lines):
        m = MARKER.match(line)
        if not m:
            continue
        for nxt in lines[i + 1:i + 6]:
            if nxt.strip().startswith("//") or not nxt.strip():
                continue
            d = DEFN.findall(nxt)
            if d:
                funcs.append((int(m.group(1), 16), d[0] if d[0] not in ("static", "pascal") else d[1]))
            break
    return flags, funcs


def file_compiler(path: str) -> str | None:
    """The file's `// COMPILER:` id, or None for the project compiler."""
    lines = open(path, encoding="mac-roman", errors="replace").read().splitlines()
    for line in lines[:30]:
        m = COMPILER.match(line)
        if m:
            return m.group(1)
    return None


def compile_object(cfg: dict, src: str, flags: list[str], compiler: str) -> str:
    """Compile SRC to build/<stem>.o; returns the object."""
    build = os.path.join(ROOT, "build")
    os.makedirs(build, exist_ok=True)
    stem = os.path.splitext(os.path.basename(src))[0]
    obj = os.path.join(build, stem + ".o")
    comp = cfg["build"]["compilers"][compiler]
    env = dict(os.environ)
    # Project headers are user paths (searched by #include "..."); toolchain headers are
    # system paths via MWCIncludes, so <Resources.h> can never resolve to a project file.
    user, system = [os.path.dirname(os.path.abspath(src))], []
    for p in cfg["build"].get("includes", []):
        (user if p.startswith("sources") else system).append(os.path.join(ROOT, p))
    env["MWCIncludes"] = ";".join(system)
    if os.path.exists(obj):
        os.remove(obj)
    if comp.startswith("mpw:"):
        sys.path.insert(0, os.path.join(TOOLS, "mpwhost"))
        import mpwhost
        # a separately built library part (its own `// COMPILER:`): that compiler's own CIncludes and the cw8 shim
        cmd = ["mpwhost", comp[4:]]
        ok, text = mpwhost.compile(os.path.join(ROOT, comp[4:]), src, obj,
                                   [u for u in user if os.path.normpath(u).startswith(os.path.join(ROOT, "sources"))],
                                   list(flags), os.path.join(ROOT, "sources", "include", "cw8"), ROOT)
        r = subprocess.CompletedProcess(cmd, 0 if ok else 1, "", text)
    elif comp.startswith("emu:"):
        sys.path.insert(0, os.path.join(TOOLS, "machost"))
        import machost
        cmd = ["machost", comp[4:]]
        r = machost.compile("ppc", src, obj, user, system, list(flags), ROOT, env)
    else:
        cmd = [os.path.join(ROOT, comp, "MWCPPC.exe"), "-nolink", *[a for u in user for a in ("-i", u)],
               *flags, "-o", obj, src]
        r = subprocess.run(cmd, capture_output=True, text=True, env=env)
    out = (r.stdout + r.stderr).strip()
    if r.returncode != 0 or not os.path.exists(obj):
        sys.exit(f"COMPILE FAILED ({' '.join(cmd)} {' '.join(flags)} {src})\n{out}")
    if out:
        print(out)
    return obj


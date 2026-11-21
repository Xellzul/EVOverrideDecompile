"""The build root the kit's build tools run in: buildkit/tools/link.py and link68.py resolve every path from the
directory above buildkit/, so the kit gives them one on the container's own disk:

  sources/ buildkit/tools/ buildkit/decomp.toml   copied from the repository (read once, never written)
  toolchain/   a tree of links to the image's /opt/evo/toolchain, laid out as on the Windows machine the builds were
               pinned on; each Windows program in it (MWLinkPPC.exe, ...) is a launcher that runs the real one under Wine
  game/        a link to the image's /opt/evo/game (the user's own copies)
  build/       what the tools write; it starts with build/machost/panels-*.txt from buildkit/panels/

buildkit/tools/machost runs the CW Pro 3 Mac plugins with the preference panels their Windows twins (PPCCCompiler.dll,
68KCCompiler.dll) give for a flag set, read from build/machost by a key of the flags.
buildkit/panels/ holds them for the flag sets of the two builds, dumped on the pinning machine, so the kit needs
neither the CW Pro 3 Windows CD nor a Windows host. A flag set it lacks (a new `// COMPILE:` line on a Pro30mac file)
stops the build with the missing file's name: dump its panels on Windows and add the file."""
import os
import shutil
import stat
from pathlib import Path

from .host import KIT, REPO, BuildError, say

COPY = ["sources", "buildkit/tools", "buildkit/decomp.toml"]

LAUNCHER = '''#!/usr/bin/env python3
"""build kit launcher: runs %(name)s under Wine; Unix paths in its arguments and in MWCIncludes become Z: paths."""
import os, sys
REAL = %(real)r
LICENSE = %(license)r


def win(a):
    return "Z:" + a.replace("/", "\\\\") if a.startswith("/") else a


env = dict(os.environ)
if env.get("MWCIncludes"):
    env["MWCIncludes"] = ";".join(win(p) for p in env["MWCIncludes"].split(";"))
if LICENSE:
    env.setdefault("LM_LICENSE_FILE", win(LICENSE))
    env.setdefault("METROWKS_LICENSE_FILE", win(LICENSE))
env.setdefault("WINEDEBUG", "-all")
os.execvpe("wine", ["wine", REAL] + [win(a) for a in sys.argv[1:]], env)
'''


def copy_repo(root):
    for rel in COPY:
        src, dst = REPO / rel, root / rel
        if not src.exists():
            raise BuildError("the repository has no %s" % rel)
        if src.is_dir():
            shutil.copytree(src, dst, ignore=shutil.ignore_patterns("__pycache__", "*.pyc"))
        else:
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)


def launcher(path, real, license_):
    path.unlink(missing_ok=True)
    path.write_text(LAUNCHER % {"name": path.name, "real": str(real), "license": str(license_ or "")})
    path.chmod(path.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)


def farm(src, dst, license_):
    """DST mirrors SRC: folders made, files linked, every .exe a Wine launcher for the real one."""
    n = 0
    for dp, dns, fns in os.walk(src):
        rel = Path(dp).relative_to(src)
        (dst / rel).mkdir(parents=True, exist_ok=True)
        for f in fns:
            s, d = Path(dp) / f, dst / rel / f
            if f.lower().endswith(".exe"):
                launcher(d, s, license_)
            else:
                d.symlink_to(s)
            n += 1
    return n


def seed_panels(root):
    """the CW Pro 3 plugins' preference panels the builds' flag sets give, into the cache tools/machost reads first"""
    d = root / "build" / "machost"
    d.mkdir(parents=True, exist_ok=True)
    for f in (KIT / "panels").glob("panels-*.txt"):
        shutil.copy2(f, d / f.name)


def make(cfg, name):
    """A fresh build root for target NAME; returns its path."""
    root = cfg.path("paths", "work") / name
    if root.exists():
        shutil.rmtree(root)
    root.mkdir(parents=True)
    copy_repo(root)
    n = farm(cfg.path("paths", "toolchain"), root / "toolchain", cfg.path("tools", "license"))
    (root / "game").symlink_to(cfg.path("paths", "game"))
    (root / "build").mkdir()
    seed_panels(root)
    say("  build root %s (%d toolchain files)" % (root, n))
    return root

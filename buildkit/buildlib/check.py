"""What `build.py check` verifies: every file unpack/layout.tsv places in the image (toolchain/ and game/), by size
and SHA-256, and the programs the builds run (Wine, Python with unicorn)."""
import shutil
from pathlib import Path

from .host import KIT, BuildError, run, say, sha256


def layout():
    rows = []
    for line in open(KIT / "unpack" / "layout.tsv", encoding="utf-8"):
        if line.startswith("#") or not line.strip():
            continue
        dest, size, sha, src, member, fork, targets = line.rstrip("\n").split("\t")
        rows.append((dest, int(size), sha, src, member, fork, targets.split()))
    return rows


def check(cfg, targets):
    """Print the problems and a count of what is in place; return the number of problems."""
    bad = ok = 0
    roots = {"toolchain": cfg.path("paths", "toolchain"), "game": cfg.path("paths", "game")}
    for dest, size, sha, src, member, fork, tg in layout():
        if not set(tg) & set(targets):
            continue
        top, rel = dest.split("/", 1)
        p = roots[top] / rel
        if not p.is_file():
            say("  MISSING  %s (from %s %s)" % (p, src, member))
            bad += 1
        elif p.stat().st_size != size or sha256(p) != sha:
            say("  WRONG    %s: %d B SHA-256 %s, expected %d B %s" % (p, p.stat().st_size, sha256(p), size, sha))
            bad += 1
        else:
            ok += 1
    say("  ok       %d files of toolchain/ and game/, each its expected size and SHA-256" % ok)
    try:
        import unicorn
        say("  ok       Python module unicorn %s" % getattr(unicorn, "__version__", ""))
    except ImportError:
        say("  MISSING  Python module unicorn")
        bad += 1
    if not shutil.which("wine"):
        say("  MISSING  wine")
        bad += 1
    else:
        try:
            rc, out = run(["wine", "--version"], timeout=120)
            say("  %s %s" % ("ok      " if rc == 0 else "FAIL    ", (out.strip().splitlines() or ["wine"])[0]))
            bad += rc != 0
        except BuildError as e:
            say("  FAIL     %s" % e)
            bad += 1
    lic = cfg.path("tools", "license")
    if not Path(lic).is_file():
        say("  MISSING  %s" % lic)
        bad += 1
    return bad

#!/usr/bin/env python3
"""unpack.py INPUTS DEST -- lay out every tool, library, header and game copy the kit reads, from the original
downloads in one folder (input-files/README.md lists them), the way their installers and discs hold them, without
running any. Run by the Dockerfile; DEST is /opt/evo, which then holds toolchain/ and game/ as the repository's own
tools expect them (layout.tsv: each file, its size and SHA-256, and the member of which download it is).

Every input is optional: a target is laid out when all of its inputs are there, and DEST/targets.txt lists those."""
import hashlib
import shutil
import sys
from pathlib import Path

import sources

HERE = Path(__file__).resolve().parent
TARGETS = ["evo"]


def read_layout():
    rows = []
    for line in open(HERE / "layout.tsv", encoding="utf-8"):
        if line.startswith("#") or not line.strip():
            continue
        dest, size, sha, src, member, fork, targets = line.rstrip("\n").split("\t")
        rows.append((dest, int(size), sha, src, member, fork, targets.split()))
    return rows


def file_sha(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 22), b""):
            h.update(b)
    return h.hexdigest()


def check_inputs(src):
    """the inputs present, each checked; exits on a wrong copy"""
    have, bad = set(), 0
    for key, (name, sha, _op) in sources.SOURCES.items():
        p = src / name
        if not p.is_file():
            continue
        ok = file_sha(p) == sha
        print("  %s %s" % ("ok      " if ok else "WRONG   ", name), flush=True)
        bad += not ok
        have.add(key)
    if bad:
        sys.exit("unpack: an input is not the expected copy (input-files/README.md lists them)")
    return have


def main(src, dest):
    src, dest = Path(src), Path(dest)
    rows = read_layout()
    print("checking the inputs in %s ..." % src, flush=True)
    have = check_inputs(src)
    targets, used = [], set()
    for t in TARGETS:
        need = sorted({r[3] for r in rows if t in r[6] and r[3] != "-"})
        gone = [sources.SOURCES[k][0] for k in need if k not in have]
        if gone:
            print("  %-8s left out: no %s" % (t, ", ".join(gone)))
        else:
            targets.append(t)
            used |= set(need)
    want = [r for r in rows if set(r[6]) & set(targets)]
    for key in [k for k in sources.SOURCES if k in used]:
        print("unpacking %s" % key, flush=True)
        s = sources.open_source(key, src, {r[4] for r in want if r[3] == key})
        n = 0
        for dest_rel, size, sha, k, member, fork, _t in want:
            if k != key:
                continue
            try:
                b = s.read(member, fork)
            except KeyError:
                if fork != "finf":
                    raise
                print("  note: %s has no Finder info for %s: the build's .finf/ stays empty" % (key, member))
                continue
            if len(b) != size or hashlib.sha256(b).hexdigest() != sha:
                sys.exit("unpack: %s %s (%s) is not the expected file for %s" % (key, member, fork, dest_rel))
            p = dest / dest_rel
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(b)
            n += 1
        print("  %d files" % n, flush=True)
        del s
        shutil.rmtree(sources.WORK / key, ignore_errors=True)
    for dest_rel, *_r in [r for r in want if r[3] == "-"]:
        p = dest / dest_rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(b"")
    shutil.rmtree(sources.WORK, ignore_errors=True)
    dest.mkdir(parents=True, exist_ok=True)
    (dest / "targets.txt").write_text("".join(t + "\n" for t in targets))
    print("unpack: done; targets: %s" % (" ".join(targets) or "none"))


if __name__ == "__main__":
    main(*sys.argv[1:3])

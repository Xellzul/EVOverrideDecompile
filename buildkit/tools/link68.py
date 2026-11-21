"""Build the 68k half end to end and compare it with the target (the 68k twin of link.py).

    python buildkit/tools/link68.py

1. compile every sources/game/*.c and sources/lib/*68k.c for 68k (cw68k.py): the game files with the CW Pro 3 CD's
   Mac-hosted 68K plugin and preset `evo`, the Mac Libraries part's ([link68] parts, near_data) each with its own
   compiler, flags and headers; objects land in build/68k/obj;
2. link with the CW Pro 3 CD's own IDE 68K linker plugin under machost/ (the project is [link68] ide_segments);
3. take CODE/DATA from the linker's resource fork, rebuild the user's fork with ours substituted (forkbuild.py)
   into build/EV Override.rsrc;
4. report per CODE/DATA resource: size ours/target and the first difference.
"""
import glob
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tomllib

TOOLS = os.path.dirname(os.path.abspath(__file__))                   # buildkit/tools
ROOT = os.path.dirname(os.path.dirname(TOOLS))                        # the repository
sys.path.insert(0, TOOLS)
import cw68k  # noqa: E402
import forkbuild  # noqa: E402
import rsrc  # noqa: E402

BUILD = os.path.join(ROOT, "build")
OBJ = os.path.join(cw68k.SCR, "obj")
OUT = os.path.join(BUILD, "link68")


def p(rel):
    return os.path.join(ROOT, *rel.split("/"))


def config():
    return tomllib.load(open(os.path.join(os.path.dirname(TOOLS), "decomp.toml"), "rb"))


def near_prefs(cfg):
    """The evo panel for the Mac Libraries part: [link68] near_panel (68K CodeGen byte -> value), by default
    'far data' (byte 16) off so the part keeps its globals near; byte 2 is the code model (0 near, 1 smart, 2 far)."""
    b = bytearray.fromhex(cw68k.PRESET_PREFS["evo"])
    for k, v in cfg["link68"].get("near_panel", {"16": 0}).items():
        b[int(k)] = v
    return b.hex()


def near_tree(cfg):
    """[link68] near_includes: the Mac Libraries part's Universal Interfaces (as on PPC, an older UI than the
    game's), staged with a MacTypes.h; returns its CIncludes."""
    ui = cfg["link68"].get("near_includes")
    if not ui:
        return None
    root = os.path.join(OUT, "ui_" + ui.replace("/", "_").replace(":", ""))
    ci = os.path.join(root, "MacOS Support", "Universal", "Interfaces", "CIncludes")
    if not os.path.isdir(ci):
        shutil.copytree(p(ui), ci)
        if not os.path.exists(os.path.join(ci, "MacTypes.h")):  # UI 3 name of UI 2's Types.h (the project headers use it)
            open(os.path.join(ci, "MacTypes.h"), "w").write("#include <Types.h>\n")
    return ci


def compile_all(cfg):
    near = set(cfg["link68"].get("near_data", []))
    failed = []
    flags = cw68k.PRESETS["evo"].split()
    os.makedirs(OBJ, exist_ok=True)
    srcs = sorted(glob.glob(p("sources/game/*.c")))
    srcs += sorted(glob.glob(p("sources/lib/*68k.c")))  # 68k-only library members (SATAddOns68k: CREATEBWOFFSCREEN)
    for _, objs in cfg["link68"]["ide_segments"]:  # "dummyN": code the linker strips; it only keeps segment N allocated
        for o in objs:
            if o.startswith("dummy"):
                d = os.path.join(OUT, o + ".c")
                open(d, "w").write("void %s(void) { }\n" % o)
                srcs.append(d)
    for src in srcs:
        stem = os.path.splitext(os.path.basename(src))[0]
        dst = os.path.join(OBJ, stem + ".o")
        part = cfg["link68"].get("parts", {}).get(stem, {})
        if part.get("compiler", "").startswith("mpw68:"):
            ok, msg = cw68k.mpw68_compile(p(part["compiler"][6:]), src, dst, part.get("flags", []))
            if not ok:
                print("COMPILE FAILED", stem, msg[-400:].replace("\n", " | "))
                failed.append(stem)
            else:
                print("compiled", stem, "(%s)" % part["compiler"], flush=True)
            continue
        prefs = near_prefs(cfg) if stem in near else cw68k.PRESET_PREFS["evo"]
        ui = near_tree(cfg) if stem in near else None
        try:
            obj = cw68k.compile(src, flags + part.get("flags", []), prefs, ui)
        except SystemExit as e:
            print("COMPILE FAILED", stem, str(e)[:400].replace("\n", " | "))
            failed.append(stem)
            continue
        shutil.copy(obj, dst)
        print("compiled", stem, flush=True)
    if failed:
        sys.exit("not linking: %s failed to compile" % ", ".join(failed))


def link_ide(cfg):
    """The CD's own IDE 68K linker plugin under machost/mwlink68k3.py.  The project is [link68] ide_segments (the
    groups in order: ids 1..n, an empty group keeps its id and its `__segtable__` slot stays 1); an entry is a library
    file (opened by the linker, loaded after every compiled source) if it is `lib:X` or listed in ide_libraries,
    otherwise a compiled source whose object the IDE stores.  Returns the linker's resource fork."""
    sys.path.insert(0, os.path.join(TOOLS, "machost"))
    import machost
    c = cfg["link68"]
    work = os.path.join(OUT, "w")
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)
    libs = set(c.get("ide_libraries", []))
    files, staged = [], set()
    for k, (name, objs) in enumerate(c["ide_segments"]):
        for o in objs:
            if o.startswith("lib:"):
                src, nm, lib = p(c["lib_" + o[4:]]), o[4:] + ".lib", True
            else:
                src, nm, lib = os.path.join(OBJ, o + ".o"), o + ".o", o in libs
            if nm in staged:
                sys.exit("duplicate input name %s" % nm)
            staged.add(nm)
            shutil.copyfile(src, os.path.join(work, nm))
            files.append([nm, k + 1, lib])
    env = dict(os.environ, HOST68K_PREFS=cw68k.PRESET_PREFS["evo"])
    panels = {}
    for line in open(machost.panels("68k", cw68k.PRESETS["evo"].split(), ROOT, env)):  # the game's panels, Mac byte order
        n, h = line[:22].strip(), line[22:].strip()
        if n in ("68K CodeGen", "68K Linker", "CFM68K") and h:
            d = bytearray(bytes.fromhex(h))
            if n != "CFM68K":
                d[0], d[1] = d[1], d[0]
            if n == "68K CodeGen":
                d[14], d[15] = d[15], d[14]
            if n == "68K Linker":
                d[10] = 1  # link map (EVO.MAP)
            panels[n] = d.hex()
    spec = os.path.join(OUT, "ide.json")
    json.dump({"out": "EVO", "segments": [[n, 0x28] for n, _ in c["ide_segments"]], "files": files, "panels": panels},
              open(spec, "w"), indent=1)
    plug = os.path.join(ROOT, "toolchain", "mwcppc", "compilers", "Pro30mac", "MacOS_68K_Linker")
    r = subprocess.run([sys.executable, "-I", os.path.join(TOOLS, "machost", "mwlink68k3.py"), plug, work, spec],
                       capture_output=True, text=True, errors="replace")
    text = r.stdout + r.stderr
    open(os.path.join(OUT, "link.log"), "w", encoding="utf-8").write(text)
    fk = os.path.join(work, "EVO.rsrc")
    if r.returncode or not os.path.exists(fk):
        sys.exit("LINK FAILED\n" + text[-4000:])
    undef = [l for l in text.splitlines() if "undefined" in l.lower()]
    if undef:
        print("link warnings:", *undef[:10], sep="\n  ")
    return fk


def code_data(fork):
    return {(r.type, r.id): r.data for r in rsrc.load(fork).resources if r.type in ("CODE", "DATA")}


def report(ours, tgt):
    print("\n%-10s %9s %9s  first diff" % ("resource", "ours", "target"))
    for k in sorted(set(ours) | set(tgt)):
        a, b = ours.get(k, b""), tgt.get(k, b"")
        fd = next((i for i, (x, y) in enumerate(zip(a, b)) if x != y), None)
        if fd is None and len(a) != len(b):
            fd = min(len(a), len(b))
        print("%-10s %9d %9d  %s" % ("%s %d" % k, len(a), len(b), "none" if fd is None else "0x%X" % fd))


def main():
    cfg = config()
    os.makedirs(OUT, exist_ok=True)
    compile_all(cfg)
    repl = code_data(link_ide(cfg))
    target = p(cfg["target"]["resource_fork"])
    # every other resource, the header's reserved bytes 16-127 and the rest of the map: the pinned fork's
    built = forkbuild.build(rsrc.load(target), repl)
    out = os.path.join(BUILD, "EV Override.rsrc")
    open(out, "wb").write(built)
    h = hashlib.sha256(built).hexdigest()
    print("wrote %s: %d B, sha256 %s%s" % (out, len(built), h[:16], " = target" if h == cfg["target"]["resource_fork_sha256"] else ""))
    report(repl, code_data(target))


if __name__ == "__main__":
    main()

"""Build the PPC data fork end to end and compare it with the target.

    python buildkit/tools/link.py

1. compile every sources/**/*.c with buildkit/decomp.toml's compiler and flags (cwppc.py; a file's `// COMPILE:`
   line replaces the flags, its `// COMPILER:` the compiler; -Cpp_exceptions off is appended when absent), in
   link order (first FUNCTION marker address);
2. stage the prebuilt runtime libraries ([link] runtime_libs), SAT and the stub libraries;
3. link with [link] linker (every symbol must resolve);
4. patch the stamp, write build/EV Override.pef;
5. report the first differing offset against the target (and the function holding it).
"""
from __future__ import annotations

import os
import re
import shutil
import struct
import subprocess
import sys

TOOLS = os.path.dirname(os.path.abspath(__file__))                   # buildkit/tools
ROOT = os.path.dirname(os.path.dirname(TOOLS))                        # the repository
sys.path.insert(0, TOOLS)
import cwppc  # noqa: E402

BUILD = os.path.join(ROOT, "build")
OBJ = os.path.join(BUILD, "obj")
LNK = os.path.join(BUILD, "link")
PEF_OUT = os.path.join(BUILD, "EV Override.pef")


def p(rel: str) -> str:
    return os.path.join(ROOT, *rel.split("/"))


def run(cmd, cwd=None):
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=cwd, encoding="latin-1")
    return r.returncode, (r.stdout + r.stderr)


def source_files(roots) -> list[tuple[int, str]]:
    found, dataonly = [], []
    for root in roots:
        for d, _, fs in os.walk(p(root)):
            for f in fs:
                if not f.endswith(".c"):
                    continue
                path = os.path.join(d, f)
                _, funcs = cwppc.read_source(path)
                if funcs:
                    found.append((min(a for a, _ in funcs), path))
                else:
                    # A data-only object has no FUNCTION marker: `// LINK-AFTER: <File>.c` places
                    # it right behind that file in link order.
                    m = re.search(r"^// LINK-AFTER:\s*(.+?)\s*$", open(path, encoding="latin-1").read(), re.M)
                    if m:
                        dataonly.append((m.group(1), path))
    for after, path in dataonly:
        base = next((a for a, f in found if os.path.basename(f) == after), None)
        if base is None:
            sys.exit(f"{path}: LINK-AFTER {after} names no source file with functions")
        found.append((base + 0.5, path))
    return sorted(found)


def compile_sources(cfg, files):
    """Returns [(src, obj)]; exits if any source fails to compile."""
    os.makedirs(OBJ, exist_ok=True)
    out, failed = [], []
    for _, src in files:
        stem = os.path.splitext(os.path.basename(src))[0]
        obj = os.path.join(OBJ, stem.replace(" ", "_") + ".o")
        flags, _ = cwppc.read_source(src)
        if flags is None:
            flags = cfg["build"]["default_flags"].split()
        # CW7 Gold's MPW MWCPPC 1.3 predates the option and rejects it ("unexpected parameter");
        # its objects carry no exception hunks anyway.
        if not any(f.startswith("-Cpp_exceptions") for f in flags) and cwppc.file_compiler(src) != "CW7":
            flags = flags + ["-Cpp_exceptions", "off"]
        compiler = cwppc.file_compiler(src) or cfg["build"]["compiler"]
        try:
            shutil.copyfile(cwppc.compile_object(cfg, src, flags, compiler), obj)
        except SystemExit as e:
            print(f"  {os.path.basename(src)}: COMPILE FAILED\n{str(e)[:400]}")
            failed.append(os.path.basename(src))
            continue
        out.append((src, obj))
        print(f"  {os.path.basename(src)}: ok")
    if failed:
        sys.exit("not linking: %s failed to compile" % ", ".join(failed))
    return out


def stage_libs(lk):
    """Copy the link inputs to build/link under ASCII names (the linker chokes on the f-hook in paths)."""
    os.makedirs(LNK, exist_ok=True)
    for i, lib in enumerate(lk["runtime_libs"]):
        if not os.path.exists(p(lib)):
            sys.exit(f"runtime lib missing: {lib} (buildkit/decomp.toml [link] runtime_libs says where it comes from)")
        shutil.copyfile(p(lib), os.path.join(LNK, f"rtlib{i}.lib"))
    stub = p(lk["stub_dir"])
    for lib in lk["stub_libs"]:
        shutil.copyfile(os.path.join(stub, lib), os.path.join(LNK, lib))
    for key, dst in (("sat_lib", "SAT_PPC.lib"), ("sat_addons", "SATAddons_PPC.lib")):
        if not os.path.exists(p(lk[key])):
            sys.exit(f"SAT lib missing: {lk[key]}")
        shutil.copyfile(p(lk[key]), os.path.join(LNK, dst))


def link(lk, objs):
    linker = os.path.join(p("toolchain/mwcppc/compilers/" + lk["linker"]), "MWLinkPPC.exe")
    cmd = [linker, "-xm", "a", "-m", "__start", "-stacksize", str(lk["stacksize"]),
           "-fragname", lk["fragname"], "-map", "map.txt", *objs]
    for lib in lk["stub_libs"]:
        if lib in lk["weak_libs"]:
            cmd.append("-wi")
        cmd.append(lib)
    return run(cmd, cwd=LNK)


def unwrap_macbin(path):
    b = open(path, "rb").read()
    dl = struct.unpack(">I", b[83:87])[0]
    return b[128:128 + dl]


def code_map(path):
    syms = {}
    for line in open(path, encoding="latin-1"):
        m = re.match(r"^([0-9A-F]{6})\tPR\s+\.?(\S+?)\s*(?:\s+file\s*=.*)?$", line.rstrip("\n"))
        if m:
            syms.setdefault(m.group(2), int(m.group(1), 16))
    return syms


def report(cfg, ours):
    tgt = open(p(cfg["target"]["data_fork"]), "rb").read()
    print(f"\nsize: ours {len(ours)}, target {len(tgt)}")
    first = next((i for i, (a, b) in enumerate(zip(ours, tgt)) if a != b), None)
    if first is None:
        print("first differing offset: none" + (" (the data fork is identical)" if len(ours) == len(tgt) else ", sizes differ"))
        return
    print(f"first differing offset: {first} (0x{first:X})")
    # section 0 of the PEF container is the code: its packed size and container offset, from its section header
    code_len, code_off = struct.unpack(">II", tgt[40 + 16:40 + 24])
    if code_off <= first < code_off + code_len:
        rel = first - code_off
        at = [(o, n) for n, o in code_map(os.path.join(LNK, "map.txt")).items() if o <= rel]
        if at:
            o, n = max(at)
            print(f"  in code at 0x{cfg['ppc']['code_base'] + rel:08X}: {n} +0x{rel - o:X}")


def main():
    cfg = cwppc.load_config()
    lk = cfg["link"]
    shutil.rmtree(LNK, ignore_errors=True)
    print("compiling sources")
    comp = compile_sources(cfg, source_files(cfg["source"]["roots"]))
    stage_libs(lk)
    # LINK-POST objects go behind the SAT libraries (before the glue)
    post = [o for s, o in comp if re.search(r"^// LINK-POST", open(s, encoding="latin-1").read(), re.M)]
    objs = [o for _, o in comp if o not in post]
    objs += [f"rtlib{i}.lib" for i in range(len(lk["runtime_libs"]))]
    objs += ["SAT_PPC.lib", "SATAddons_PPC.lib", *post]
    rc, text = link(lk, objs)
    undefined = re.findall(r"undefined '([^']*)' \((?:code|data|descriptor)\)", text)
    out_bin = os.path.join(LNK, "MWLinkPPC.out.bin")
    if rc or undefined or not os.path.exists(out_bin):
        print(text[-3000:])
        sys.exit("link failed" + (f": {len(undefined)} undefined symbols" if undefined else ""))
    fork = bytearray(unwrap_macbin(out_bin))
    fork[16:20] = struct.pack(">I", lk["stamp"])
    open(PEF_OUT, "wb").write(fork)
    report(cfg, bytes(fork))
    print(f"\nwrote {PEF_OUT}")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Build EV Override 1.0.2 from this repository's sources into a byte-identical copy of the original.

    python3 buildkit/build.py           build the data fork (PowerPC) and the resource fork (68k)
    python3 buildkit/build.py check     verify every tool, library, header and game copy the image holds

Options:
    --config FILE    another build.ini (default: buildkit/build.ini)
    --keep           keep the build root (build.ini [paths] work) for a look at the objects and link maps

The files go to buildkit/out/evo/: the data fork as the application's name, the resource fork in .rsrc/ and the
Finder info in .finf/ beside it. The two forks are built by buildkit/tools/link.py and link68.py; the Finder info is
your copy's. At the end each file's SHA-256 is printed beside the original download's (buildkit/decomp.toml):
IDENTICAL or DIFFERENT. README.md says what to do then.
"""
import argparse
import shutil
import subprocess
import sys
import time
import tomllib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from buildlib import check as checks, host, work  # noqa: E402
from buildlib.host import BuildError, say  # noqa: E402

NAME = "evo"            # the build root's, the output folder's and the logs' name; unpack/layout.tsv's target
APP = "EV Override"
# the tools' runs: (fork, arguments, the file the run writes under build/)
STEPS = [
    ("data", ["buildkit/tools/link.py"], "build/EV Override.pef"),
    ("rsrc", ["buildkit/tools/link68.py"], "build/EV Override.rsrc"),
]


def build(cfg, keep):
    tcfg = tomllib.loads((host.KIT / "decomp.toml").read_text(encoding="utf-8"))["target"]
    pin = {"data": tcfg["data_fork_sha256"], "rsrc": tcfg["resource_fork_sha256"], "finf": tcfg["finder_info_sha256"]}
    out = cfg.out(NAME)
    for sub in (".rsrc", ".finf"):
        (out / sub).mkdir(exist_ok=True)
    dest = {"data": out / APP, "rsrc": out / ".rsrc" / APP, "finf": out / ".finf" / APP}
    for f in dest.values():
        f.unlink(missing_ok=True)
    results = []
    root = work.make(cfg, NAME)
    try:
        for fork, args, made in STEPS:
            say("  %s fork: python3 %s" % ({"data": "data", "rsrc": "resource"}[fork], " ".join(args)))
            t0 = time.time()
            log = out / ("%s-%s.log" % (NAME, fork))
            rc, text = host.run([sys.executable, *args], cwd=root, env=host.env(PYTHONDONTWRITEBYTECODE="1", PYTHONUNBUFFERED="1"), log=log)
            f = root / made
            if rc or not f.is_file():
                tail = "\n".join(text.strip().splitlines()[-12:])
                say("    FAILED (exit %d; the whole output is in %s):\n%s" % (rc, log, tail))
                results.append((fork, None, pin[fork], "FAILED"))
                continue
            shutil.copyfile(f, dest[fork])
            sha = host.sha256(dest[fork])
            results.append((fork, sha, pin[fork], "IDENTICAL" if sha == pin[fork] else "DIFFERENT"))
            say("    %s, %d B, %.0f s" % (dest[fork], dest[fork].stat().st_size, time.time() - t0))
    finally:
        if not keep:
            shutil.rmtree(root, ignore_errors=True)
    finf = cfg.path("paths", "game") / Path(tcfg["finder_info"]).relative_to("game")
    if finf.is_file():
        shutil.copyfile(finf, dest["finf"])
        sha = host.sha256(dest["finf"])
        # not built: your copy's, as the download holds it
        results.append(("finf", sha, pin["finf"], "IDENTICAL (your copy's)" if sha == pin["finf"] else "DIFFERENT"))
    else:
        results.append(("finf", None, pin["finf"], "FAILED (no Finder info in your copy)"))
    return results


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0], formatter_class=argparse.RawDescriptionHelpFormatter,
                                 epilog=__doc__.split("\n\n", 1)[1])
    ap.add_argument("action", nargs="?", choices=["check"], help="check: verify every tool, library, header and game copy the image holds")
    ap.add_argument("--config", default=str(Path(__file__).resolve().parent / "build.ini"))
    ap.add_argument("--keep", action="store_true")
    a = ap.parse_args()
    cfg = host.Config(a.config)
    listed = cfg.path("paths", "targets")
    ready = listed.is_file() and NAME in listed.read_text().split()

    if a.action == "check":
        say("Checking the tools, libraries, headers and originals the image holds ...")
        bad = checks.check(cfg, [NAME] if ready else [])
        say("check: %s" % ("everything is in place" if not bad else "%d problem(s) above" % bad))
        return 1 if bad or not ready else 0

    if not ready:
        raise BuildError("the image was built without some of its inputs (input-files/README.md): rebuild it with "
                         "all of them in the folder")
    # one Wine server for the whole run: each Windows program call would otherwise start (and boot) its own
    subprocess.Popen(["wineserver", "-p"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    # boot the session here, with no pipes attached: the services Wine starts on its first program stay up with the
    # server, and would otherwise hold that program's output pipe open, so the tool waiting on it would never return
    subprocess.run(["wine", "cmd", "/c", "exit"], stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL, timeout=300)
    say("== EV Override 1.0.2 -> %s" % cfg.out(NAME))
    t0 = time.time()
    try:
        rows = build(cfg, a.keep)
    except BuildError as e:
        say("FAILED: %s" % e)
        rows = [("data", None, "-", "FAILED"), ("rsrc", None, "-", "FAILED")]
    say("   %.0f s" % (time.time() - t0))

    host.run(["wineserver", "-k"], timeout=120)
    say("")
    say("%-5s %-64s %s" % ("fork", "SHA-256 of the build", "against the original"))
    for fork, sha, pin, verdict in rows:
        say("%-5s %-64s %s" % (fork, sha or "-", verdict))
        say("%-5s %-64s %s" % ("", pin, "(the original's)"))
    return 0 if all(r[3].startswith("IDENTICAL") for r in rows) else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BuildError as e:
        say("ERROR: %s" % e)
        sys.exit(2)

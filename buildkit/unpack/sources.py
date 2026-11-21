"""The original downloads, opened the way their installers or disc images hold their files, without running any.

Each source is a set of members, each member a file with up to three forks:
  data, rsrc  its data and resource fork
  finf        its 32 bytes of Finder info (FInfo + FXInfo), where the container keeps them
A member of an HFS disc is named by its path below the volume with ':' between names (HFS names may hold '/');
a member of an extracted folder tree by its relative path with '/'.

SOURCES says how each input is opened. unpack.py copies the members layout.tsv names to their place in the toolchain
tree (layout.tsv was written by matching the pinning machine's toolchain/ against the members, by content: genlayout.py
in EvOverrideDecompile's history)."""
import os
import shutil
import struct
import subprocess
import zipfile
from pathlib import Path

import macfs
import sit5

WORK = Path("/var/tmp/evo-unpack")


def sh(*cmd, quiet=True, check=True):
    subprocess.run([str(c) for c in cmd], check=check, stdout=subprocess.DEVNULL if quiet else None,
                   stderr=subprocess.DEVNULL if not check else None)


def fresh(name):
    d = WORK / name
    shutil.rmtree(d, ignore_errors=True)
    d.mkdir(parents=True)
    return d


# -- the members of a source ---------------------------------------------------------------------
class HfsSource:
    """an HFS disc image (CD, Toast, DiskCopy-less .cdr), read in place"""

    def __init__(self, image):
        self.hfs = macfs.Hfs(image)
        self.recs = {":".join(p.replace(":", "/") for p in parts): rec for parts, rec in self.hfs.files()}

    def members(self):
        return sorted(self.recs)

    def read(self, member, fork):
        rec = self.recs[member]
        if fork == "finf":
            return bytes(rec[4:20]) + bytes(rec[56:72])
        d, r = self.hfs.forks(rec)
        return d if fork == "data" else r

    def forks(self, member):
        rec = self.recs[member]
        return [f for f, n in (("data", macfs.u32(rec, 26)), ("rsrc", macfs.u32(rec, 36))) if n]


class TreeSource:
    """an extracted folder: a plain file is its data fork; `unar -k visible` keeps a Mac file's resource fork and Finder
    info in an AppleDouble file NAME.rsrc beside it"""

    def __init__(self, root, appledouble=False):
        self.root, self.ad = Path(root), appledouble
        self.files = {}
        for dp, _, fns in os.walk(self.root):
            for f in fns:
                rel = (Path(dp) / f).relative_to(self.root).as_posix()
                self.files[rel] = Path(dp) / f
        if appledouble:
            self.pairs = {m[:-5]: p for m, p in self.files.items() if m.endswith(".rsrc") and is_appledouble(p)}
            for m in self.pairs:
                self.files.pop(m + ".rsrc", None)
                self.files.setdefault(m, None)

    def members(self):
        return sorted(self.files)

    def read(self, member, fork):
        if fork == "data":
            p = self.files[member]
            return p.read_bytes() if p else b""
        ad = self.pairs.get(member) if self.ad else None
        if ad is None:
            return b""
        return appledouble_entry(ad.read_bytes(), 2 if fork == "rsrc" else 9)

    def forks(self, member):
        out = ["data"] if self.files[member] is not None else []
        if self.ad and member in self.pairs:
            b = self.pairs[member].read_bytes()
            out += [f for f, e in (("rsrc", 2), ("finf", 9)) if appledouble_entry(b, e)]
        return out


def is_appledouble(p):
    with open(p, "rb") as f:
        return f.read(4) in (b"\x00\x05\x16\x07", b"\x00\x05\x16\x00")


def appledouble_entry(b, eid):
    for i in range(struct.unpack_from(">H", b, 24)[0]):
        e, off, ln = struct.unpack_from(">III", b, 26 + 12 * i)
        if e == eid:
            return b[off:off + ln]
    return b""


# -- containers ----------------------------------------------------------------------------------
def unzip_member(zpath, suffixes, dest):
    """the one member of the zip whose name ends with one of SUFFIXES (no __MACOSX), extracted to DEST"""
    with zipfile.ZipFile(zpath) as z:
        names = [n for n in z.namelist() if n.lower().endswith(suffixes) and "__MACOSX" not in n]
        if len(names) != 1:
            raise ValueError("%s: expected one %s member, found %s" % (zpath, suffixes, names))
        out = dest / Path(names[0]).name
        with z.open(names[0]) as s, open(out, "wb") as d:
            shutil.copyfileobj(s, d, 1 << 22)
    return out


def raw_cd_to_iso(binpath, iso):
    """a raw 2352-byte-sector CD image (.bin) to its 2048-byte user data (mode 1: from byte 16; mode 2 form 1: 24)"""
    with open(binpath, "rb") as s, open(iso, "wb") as d:
        while True:
            sec = s.read(2352)
            if len(sec) < 2352:
                break
            d.write(sec[16:2064] if sec[15] == 1 else sec[24:2072])
    return iso


def unar(src, dest, check=True):
    """StuffIt, Compact Pro, BinHex, MacBinary: forks and Finder info kept in NAME.rsrc (AppleDouble)"""
    sh("unar", "-q", "-f", "-k", "visible", "-o", dest, src, check=check)


def unar_all(src, dest):
    """unar, then unar again on what it gave while that is another Mac archive (a .cpt or .sit inside a BinHex)"""
    unar(src, dest)
    for _ in range(3):
        nested = [p for p in dest.rglob("*") if p.is_file() and p.suffix.lower() in (".sit", ".cpt", ".sea", ".hqx", ".bin")
                  and not p.name.endswith(".rsrc")]
        if not nested:
            break
        for p in nested:
            sub = p.with_name(p.name + "_x")
            sub.mkdir()
            unar(p, sub)
            p.unlink()
            r = p.with_name(p.name + ".rsrc")
            if r.exists():
                r.unlink()


# -- the inputs ----------------------------------------------------------------------------------
def open_hfs_zip(path, w, wanted=None):
    return HfsSource(unzip_member(path, (".iso", ".cdr", ".toast", ".img"), w))


def open_hfs_sit(path, w, wanted=None):
    unar(path, w)
    img = [p for p in w.rglob("*") if p.suffix.lower() in (".cdr", ".iso", ".toast", ".img") and p.is_file()]
    if len(img) != 1:
        raise ValueError("%s: expected one disc image inside, found %s" % (path, img))
    return HfsSource(img[0])


def open_hfs(path, w, wanted=None):
    return HfsSource(path)


def open_pro5(path, w, wanted=None):
    iso = raw_cd_to_iso(path, w / "CW_PRO5.iso")
    sh("7z", "e", "-y", "-o%s" % w, iso, "CodeWarrior.zip")
    iso.unlink()
    t = w / "tree"
    if wanted is None:
        sh("7z", "x", "-y", "-o%s" % t, w / "CodeWarrior.zip")
    else:
        (w / "members.txt").write_text("".join(m + "\n" for m in sorted(wanted)), encoding="utf-8")
        sh("7z", "x", "-y", "-o%s" % t, w / "CodeWarrior.zip", "-scsUTF-8", "@%s" % (w / "members.txt"))
    (w / "CodeWarrior.zip").unlink()
    return TreeSource(t)


def open_mac_archive(path, w, wanted=None):
    unar_all(path, w)
    return TreeSource(w, appledouble=True)


class EvoSource(TreeSource):
    """EV_Override_1.0.2_f.sit as unar lays it out, but the application read by sit5.py, exactly as the archive holds it:
    unar 1.10.1 decodes its data fork two bytes late, and its AppleDouble Finder info keeps the type, creator and flags
    only (the location and FXInfo zeroed). The resource fork keeps its header's 51 bytes of leftovers in bytes 16-127
    (a MacBinary name "...Installer_1.0.2.bin", APPL, the creator), which the Resource Manager reserves and never reads:
    decomp.toml pins this fork, so tools/forkbuild.py carries them from it into the build."""

    def __init__(self, root, archive):
        super().__init__(root, appledouble=True)
        self.archive = archive
        ents = sit5.entries(Path(archive).read_bytes())
        self.app = {m: e for m in self.files for e in ents if m.endswith("/EV Override") and e.endswith("/EV Override")}

    def read(self, member, fork):
        if member in self.app:
            return sit5.read(self.archive, self.app[member], fork)
        return super().read(member, fork)


def open_evo(path, w, wanted=None):
    unar(path, w, check=False)    # it reports the application's data fork as failed: EvoSource reads that one itself
    return EvoSource(w, path)


# key: (file name in input-files/, SHA-256 of the original, opener)
SOURCES = {
    "pro3mac": ("CWPro3Tools.cdr_.zip", "2f9b5d8e285c5248f8beaebcc6f68e53ec5a8d079f4ab26b9e527e1e43519e03", open_hfs_zip),
    "pro5": ("CW_PRO5.bin", "17a40c5106dd38e3da9c80d325d6102474b3cf22af22731bd5d1d93e4fc70783", open_pro5),
    "cw7": ("CW7_Gold_Tools.iso_.zip", "10a227e08081805f0f548997c912f187bf79855706d3ff5070f7ec18c62f0258", open_hfs_zip),
    "cwpro1": ("CWPro1Tools.sit", "100095550149213d40468b0556f0fa2cd9364a23a64d1ddeccd7e902a2be1e29", open_hfs_sit),
    "pro2": ("CWPro_2_Tools_199710.toast", "0e75071a386027a2fb96cb8f4ca3643e67521316975d8df88e7e1c498fa78db5", open_hfs),
    "sat260": ("sat260b-libs.hqx", "9e72bd8f1991295e14ab6d496b77204541de5341c0ae3709e7a2f60660bffb6f", open_mac_archive),
    "sat240": ("SAT-240-libdoc.hqx", "83b57c9f403c3c1a10ee03cf4c6af713ffb47678cb0da2841d687bd51010643b", open_mac_archive),
    "evo": ("EV_Override_1.0.2_f.sit", "360d700099ce05ae95c3b5601bd4a115c8da2b971df6f37e4bb8c5167b86cd72", open_evo),
}


def open_source(key, inputs, wanted=None):
    """the source KEY of the inputs folder; WANTED: the members needed (None: all), so a big tree is not extracted whole"""
    name, _sha, opener = SOURCES[key]
    return opener(Path(inputs) / name, fresh(key), wanted)

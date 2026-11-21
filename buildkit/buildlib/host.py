"""build.ini, paths and processes. The kit runs in its Linux container; the Windows programs run under Wine."""
import configparser
import hashlib
import os
import subprocess
from pathlib import Path

KIT = Path(__file__).resolve().parents[1]          # buildkit/
REPO = KIT.parent                                  # the repository: sources/ (and buildkit/)


class BuildError(Exception):
    pass


def say(*parts):
    print(*parts, flush=True)


class Config:
    """build.ini. A relative path is relative to the repository root."""

    def __init__(self, path):
        self.file = Path(path)
        self.ini = configparser.ConfigParser(interpolation=None, inline_comment_prefixes=None)
        if not self.ini.read(self.file, encoding="utf-8"):
            raise BuildError("no config file %s" % self.file)

    def get(self, section, key, default=None):
        v = self.ini.get(section, key, fallback=default)
        return v.strip() if isinstance(v, str) else v

    def path(self, section, key):
        v = self.get(section, key)
        if not v:
            raise BuildError("build.ini: [%s] %s is not set" % (section, key))
        p = Path(v)
        return p if p.is_absolute() else REPO / p

    def out(self, target):
        d = self.path("paths", "out") / target
        d.mkdir(parents=True, exist_ok=True)
        return d


def run(cmd, cwd=None, env=None, timeout=7200, log=None):
    """Run a program; returns (exit code, stdout and stderr as text). LOG: a file that gets the output as it comes."""
    try:
        if log is None:
            r = subprocess.run([str(c) for c in cmd], cwd=cwd and str(cwd), env=env, timeout=timeout,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            return r.returncode, r.stdout.decode("latin-1")
        with open(log, "wb") as f:
            r = subprocess.run([str(c) for c in cmd], cwd=cwd and str(cwd), env=env, timeout=timeout,
                               stdout=f, stderr=subprocess.STDOUT)
        return r.returncode, Path(log).read_bytes().decode("latin-1")
    except FileNotFoundError:
        raise BuildError("cannot run %s: not found" % cmd[0])
    except subprocess.TimeoutExpired:
        raise BuildError("%s timed out after %d s" % (Path(str(cmd[0])).name, timeout))


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def env(**more):
    e = dict(os.environ)
    e.update(more)
    return e

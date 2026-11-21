"""Classic Mac containers, read without mounting or running anything: HFS volumes."""
import struct


def u16(b, o):
    return struct.unpack_from(">H", b, o)[0]


def u32(b, o):
    return struct.unpack_from(">I", b, o)[0]


# -- classic HFS: a CD, a Toast, a floppy image ----------------------------------------------------
class Hfs:
    def __init__(self, path):
        self.f = open(path, "rb")
        self.base = self.find_volume()
        mdb = self.read(self.base + 1024, 162)
        self.alblksiz, self.alblst = u32(mdb, 20), u16(mdb, 28)
        self.overflow = {}
        for key, data in self.leaves(self.fork(self.extents(mdb[134:146]), u32(mdb, 130))):
            ftype, fid, start = struct.unpack(">BIH", key[1:8])
            self.overflow.setdefault((fid, ftype), []).append((start, self.extents(data[:12])))
        self.ct = self.fork(self.extents(mdb[150:162]), u32(mdb, 146))

    def read(self, off, n):
        self.f.seek(off)
        return self.f.read(n)

    def find_volume(self):
        for off in (0, 0x800):
            if self.read(off + 1024, 2) == b"BD":
                return off
        for i in range(1, 64):                     # an Apple partition map: the first Apple_HFS partition
            e = self.read(512 * i, 512)
            if e[:2] != b"PM":
                break
            if e[48:80].split(b"\0")[0] == b"Apple_HFS":
                return 512 * u32(e, 8)
        raise ValueError("no HFS volume")

    @staticmethod
    def extents(b):
        return [struct.unpack(">HH", b[4 * i:4 * i + 4]) for i in range(3) if u16(b, 4 * i + 2)]

    def fork(self, ext, length, fid=None, ftype=0):
        ext = list(ext)
        if fid is not None:
            have = sum(c for _, c in ext)
            for start, more in sorted(self.overflow.get((fid, ftype), [])):
                if start >= have:
                    ext += more
                    have += sum(c for _, c in more)
        out = bytearray()
        for s, c in ext:
            out += self.read(self.base + self.alblst * 512 + s * self.alblksiz, c * self.alblksiz)
        if len(out) < length:
            raise ValueError("short fork of file %s" % fid)
        return bytes(out[:length])

    @staticmethod
    def leaves(tree):
        nsize, node = u16(tree, 32), u32(tree, 24)
        while node:
            n = tree[node * nsize:(node + 1) * nsize]
            for r in range(u16(n, 10)):
                off = u16(n, nsize - 2 * (r + 1))
                key = n[off:off + 1 + n[off]]
                d = off + 1 + n[off]
                yield key, n[d + (d & 1):]
            node = u32(n, 0)

    def files(self):
        """(path parts below the volume, catalog record) of every file"""
        dirs, files = {}, []
        for key, data in self.leaves(self.ct):
            parent, name = u32(key, 2), key[7:7 + key[6]].decode("mac_roman")
            if data[0] == 1:
                dirs[u32(data, 6)] = (parent, name)
            elif data[0] == 2:
                files.append((parent, name, data))
        for parent, name, data in files:
            parts = [name]
            while parent in dirs:
                parent, n = dirs[parent]
                parts.append(n)
            yield list(reversed(parts))[1:], data      # [0] was the volume name

    def forks(self, data):
        fid = u32(data, 20)
        return (self.fork(self.extents(data[74:86]), u32(data, 26), fid, 0x00),
                self.fork(self.extents(data[86:98]), u32(data, 36), fid, 0xFF))

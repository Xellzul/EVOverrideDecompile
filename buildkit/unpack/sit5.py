"""StuffIt 5 archives: one fork (or the Finder info) of one file, decoded here rather than by unar.

unar 1.10.1 (Ubuntu 24.04's) decodes the data fork of EV Override in EV_Override_1.0.2_f.sit two bytes late: two zero
bytes first, the last two lost, and a CRC error. This reads the entry the way unar's XADStuffIt5Parser.m does and
decodes method 13 (LZSS with Huffman codes, XADStuffIt13Handle.m) with the dynamic code tables the entry uses; the
CRC-16 the archive stores is checked: the data fork's stream turns out to encode two zero bytes before the
fork, which the CRC (and the fork's length) leave out. The five predefined table sets of method 13 are not carried here."""
import struct

SIT5_ID = 0xA5A5A5A5
DIRECTORY, CRYPTED = 0x40, 0x20
META_CODES = [0x5d8, 0x058, 0x040, 0x0c0, 0x000, 0x078, 0x02b, 0x014, 0x00c, 0x01c, 0x01b, 0x00b, 0x010, 0x020, 0x038, 0x018,
              0x0d8, 0xbd8, 0x180, 0x680, 0x380, 0xf80, 0x780, 0x480, 0x080, 0x280, 0x3d8, 0xfd8, 0x7d8, 0x9d8, 0x1d8, 0x004,
              0x001, 0x002, 0x007, 0x003, 0x008]
META_LENGTHS = [11, 8, 8, 8, 8, 7, 6, 5, 5, 5, 5, 6, 5, 6, 7, 7, 9, 12, 10, 11, 11, 12, 12, 11, 11, 11, 12, 12, 12, 12, 12,
                5, 2, 2, 3, 4, 5]


def entries(d):
    """{path: entry} of a StuffIt 5 archive; path joins names with '/', entry has the forks' offset, sizes, method, CRC"""
    if not d.startswith(b"StuffIt (c)1997-"):
        raise ValueError("not a StuffIt 5 archive")
    numentries, first = struct.unpack_from(">HI", d, 92)
    out, dirs, o, i = {}, {}, first, 0
    while i < numentries:
        if struct.unpack_from(">I", d, o)[0] != SIT5_ID:
            raise ValueError("StuffIt 5 entry header expected at %d" % o)
        version, headersize, flags = d[o + 4], struct.unpack_from(">H", d, o + 6)[0], d[o + 9]
        diroffs, namelen = struct.unpack_from(">IH", d, o + 26)
        datalen, datacomp, datacrc = struct.unpack_from(">IIH", d, o + 34)
        p = o + 46
        if flags & DIRECTORY:
            numfiles = struct.unpack_from(">H", d, p)[0]
            p += 2
            if datalen == 0xFFFFFFFF:
                numentries += 1
                i += 1
                o = p
                continue
            datamethod = None
        else:
            datamethod, passlen = d[p], d[p + 1]
            p += 2 + (passlen if flags & CRYPTED and datalen else 0)
        name = d[p:p + namelen].decode("mac_roman")   # (this archive writes UTF-8 names; the caller matches either)
        p += namelen
        if p < o + headersize:
            p += 4 + struct.unpack_from(">H", d, p)[0]
        something = struct.unpack_from(">H", d, p)[0]
        # version 1 keeps the whole Finder info here: FInfo (type, creator, flags, location, folder) and FXInfo, 32 bytes
        finf = bytes(d[p + 4:p + 36]) if version == 1 else None
        p += 4 + 4 + 4 + 2 + (22 if version == 1 else 18)
        rlen = rcomp = rcrc = rmethod = 0
        if something & 1:
            rlen, rcomp, rcrc = struct.unpack_from(">IIH", d, p)
            rmethod, passlen = d[p + 12], d[p + 13]
            p += 14 + (passlen if flags & CRYPTED and rlen else 0)
        path = (dirs[diroffs] + "/" if diroffs in dirs else "") + name
        if flags & DIRECTORY:
            dirs[o] = path
            numentries += numfiles
            o = p
        else:
            out[path] = {"rsrc": (p, rcomp, rlen, rmethod, rcrc), "data": (p + rcomp, datacomp, datalen, datamethod, datacrc),
                         "finf": finf}
            o = p + rcomp + datacomp
        i += 1
    return out


class Bits:
    """bits taken low bit first from each byte"""

    def __init__(self, b, pos):
        self.b, self.pos, self.acc, self.n = b, pos, 0, 0

    def bit(self):
        if not self.n:
            self.acc, self.n = self.b[self.pos], 8
            self.pos += 1
        v = self.acc & 1
        self.acc >>= 1
        self.n -= 1
        return v

    def bits(self, n):
        v = 0
        for k in range(n):
            v |= self.bit() << k
        return v


def code_low_bit_first(codes, lengths):
    t = {}
    for sym, (c, n) in enumerate(zip(codes, lengths)):
        t[(n, int(format(c, "0%db" % n)[::-1], 2))] = sym     # as read: first bit is the most significant of the key
    return t


def canonical(lengths):
    """XADPrefixCode initWithLengths:shortestCodeIsZeros:YES; codes read high bit first"""
    t, code, left = {}, 0, sum(1 for n in lengths if n > 0)
    for n in range(1, 33):
        for sym, ln in enumerate(lengths):
            if ln == n:
                t[(n, code)] = sym
                code += 1
                left -= 1
        if not left:
            break
        code <<= 1
    return t


def symbol(bits, table):
    v = n = 0
    while n < 33:
        v = (v << 1) | bits.bit()
        n += 1
        if (n, v) in table:
            return table[(n, v)]
    raise ValueError("bad prefix code")


def parse_code(bits, size, meta):
    lengths, length, i = [0] * (size + 70), 0, 0
    while i < size:
        val = symbol(bits, meta)
        if val == 31:
            length = -1
        elif val == 32:
            length += 1
        elif val == 33:
            length -= 1
        elif val == 34:
            if bits.bit():
                lengths[i] = length
                i += 1
        elif val == 35:
            for _ in range(bits.bits(3) + 2):
                lengths[i] = length
                i += 1
        elif val == 36:
            for _ in range(bits.bits(6) + 10):
                lengths[i] = length
                i += 1
        else:
            length = val + 1
        lengths[i] = length
        i += 1
    return canonical(lengths[:size])


def method13(d, pos, size):
    first = d[pos]
    if first >> 4:
        raise ValueError("StuffIt method 13 with predefined code tables (set %d): not supported here" % (first >> 4))
    bits = Bits(d, pos + 1)
    meta = code_low_bit_first(META_CODES, META_LENGTHS)
    firstcode = parse_code(bits, 321, meta)
    secondcode = firstcode if first & 0x08 else parse_code(bits, 321, meta)
    offsetcode = parse_code(bits, (first & 0x07) + 10, meta)
    out, cur = bytearray(), firstcode
    while len(out) < size:
        val = symbol(bits, cur)
        if val < 0x100:
            out.append(val)
            cur = firstcode
            continue
        cur = secondcode
        if val < 0x13E:
            length = val - 0x100 + 3
        elif val == 0x13E:
            length = bits.bits(10) + 65
        elif val == 0x13F:
            length = bits.bits(15) + 65
        else:
            break
        bl = symbol(bits, offsetcode)
        offset = 1 if bl == 0 else 2 if bl == 1 else (1 << (bl - 1)) + bits.bits(bl - 1) + 1
        for _ in range(length):
            out.append(out[-offset] if offset <= len(out) else 0)
    return bytes(out[:size])


def crc16(b):
    c = 0
    for x in b:
        c ^= x
        for _ in range(8):
            c = (c >> 1) ^ 0xA001 if c & 1 else c >> 1
    return c


def read(path, member, fork):
    """FORK ("data", "rsrc" or "finf", the 32 bytes of Finder info) of the file MEMBER ('/'-joined names) of the
    StuffIt 5 archive at PATH"""
    d = open(path, "rb").read()
    if fork == "finf":
        finf = entries(d)[member]["finf"]
        if finf is None:
            raise ValueError("%s: a version 2 entry, whose Finder info is not read here" % member)
        return finf
    off, comp, size, method, crc = entries(d)[member][fork]
    if method == 0:
        out = d[off:off + size]
    elif method == 13:
        out = method13(d, off, size + 2)
        # EV Override's data fork stream starts with two zero literals the CRC does not cover (unar 1.10.1 keeps them,
        # cuts the fork's last two bytes and reports a CRC error): drop them when that is what the CRC says
        out = out[2:] if crc16(out[:size]) != crc and crc16(out[2:]) == crc else out[:size]
    else:
        raise ValueError("StuffIt method %d: not supported here" % method)
    if crc16(out) != crc:
        raise ValueError("%s %s: CRC-16 %04x, the archive says %04x" % (member, fork, crc16(out), crc))
    return out

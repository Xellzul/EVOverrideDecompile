"""A small SANE (Apple Numerics) model: extended/double/single/integer conversion and arithmetic,
round-to-nearest-even, exact via Fraction.  Values: ('f', sign, Fraction>=0) | ('i', sign) | ('n',) """
from fractions import Fraction
import struct

NAN = ('n',)


def rnd_even(q):            # Fraction >= 0 -> int
    n, d = q.numerator, q.denominator
    f, r = divmod(n, d)
    if 2 * r > d or (2 * r == d and f & 1):
        f += 1
    return f


def round_to(q, p, emin, emax):
    """Round Fraction q>0 to p-bit significand; exponent range emin..emax (unbiased, value = m * 2^(e-p+1)).
    Returns (m, e) with m < 2^p, or None on overflow, denormals have e == emin and m < 2^(p-1)."""
    e = q.numerator.bit_length() - q.denominator.bit_length()
    if Fraction(2) ** e > q:
        e -= 1
    elif Fraction(2) ** (e + 1) <= q:
        e += 1
    if e < emin:
        e = emin
    m = rnd_even(q / Fraction(2) ** (e - p + 1))
    if m >= 1 << p:
        m >>= 1
        e += 1
    if e > emax:
        return None
    return m, e


def dec_ext(b):
    se = struct.unpack('>H', b[:2])[0]
    s = se >> 15
    ex = se & 0x7fff
    m = struct.unpack('>Q', b[2:10])[0]
    if ex == 0x7fff:
        return ('i', s) if (m << 1) & ((1 << 64) - 1) == 0 else NAN
    if ex == 0:
        ex = 1
    return ('f', s, Fraction(m) * Fraction(2) ** (ex - 16383 - 63))


def enc_ext(v):
    if v[0] == 'n':
        return struct.pack('>HQ', 0x7fff, 0xC000000000000000 | 1)
    if v[0] == 'i':
        return struct.pack('>HQ', (v[1] << 15) | 0x7fff, 1 << 63)
    s, q = v[1], v[2]
    if q == 0:
        return struct.pack('>HQ', s << 15, 0)
    r = round_to(q, 64, -16382, 16383)
    if r is None:
        return struct.pack('>HQ', (s << 15) | 0x7fff, 1 << 63)
    m, e = r
    be = e + 16383
    if m < 1 << 63:
        be = 0
    return struct.pack('>HQ', (s << 15) | be, m)


def dec_dbl(b):
    x = struct.unpack('>d', b[:8])[0]
    if x != x:
        return NAN
    if x in (float('inf'), float('-inf')):
        return ('i', 1 if x < 0 else 0)
    s = 1 if (b[0] & 0x80) else 0
    return ('f', s, abs(Fraction(x)))


def enc_dbl(v):
    if v[0] == 'n':
        return struct.pack('>Q', 0x7ff8000000000000)
    if v[0] == 'i':
        return struct.pack('>Q', (v[1] << 63) | 0x7ff0000000000000)
    s, q = v[1], v[2]
    if q == 0:
        return struct.pack('>Q', s << 63)
    r = round_to(q, 53, -1022, 1023)
    if r is None:
        return struct.pack('>Q', (s << 63) | 0x7ff0000000000000)
    m, e = r
    be = e + 1023
    if m < 1 << 52:
        be = 0
    return struct.pack('>Q', (s << 63) | (be << 52) | (m & ((1 << 52) - 1)))


def dec_sgl(b):
    x = struct.unpack('>f', b[:4])[0]
    if x != x:
        return NAN
    if x in (float('inf'), float('-inf')):
        return ('i', 1 if x < 0 else 0)
    return ('f', 1 if (b[0] & 0x80) else 0, abs(Fraction(x)))


def enc_sgl(v):
    if v[0] == 'n':
        return struct.pack('>I', 0x7fc00000)
    if v[0] == 'i':
        return struct.pack('>I', (v[1] << 31) | 0x7f800000)
    s, q = v[1], v[2]
    if q == 0:
        return struct.pack('>I', s << 31)
    r = round_to(q, 24, -126, 127)
    if r is None:
        return struct.pack('>I', (s << 31) | 0x7f800000)
    m, e = r
    be = e + 127
    if m < 1 << 23:
        be = 0
    return struct.pack('>I', (s << 31) | (be << 23) | (m & ((1 << 23) - 1)))


def to_int(v, bits):
    lo, hi = -(1 << (bits - 1)), (1 << (bits - 1)) - 1
    if v[0] != 'f':
        return lo
    n = rnd_even(v[2])
    n = -n if v[1] else n
    return n if lo < n <= hi else lo


def from_int(n):
    return ('f', 1 if n < 0 else 0, Fraction(abs(n)))


def neg(v):
    if v[0] == 'n':
        return v
    return (v[0], 1 - v[1]) + v[2:]


def val(v):
    return (-1 if v[1] else 1) * v[2]


def add(a, b):
    if a[0] == 'n' or b[0] == 'n':
        return NAN
    if a[0] == 'i' or b[0] == 'i':
        if a[0] == 'i' and b[0] == 'i':
            return a if a[1] == b[1] else NAN
        return a if a[0] == 'i' else b
    x = val(a) + val(b)
    if x == 0:
        return ('f', 1 if (a[1] and b[1]) else 0, Fraction(0))
    return ('f', 1 if x < 0 else 0, abs(x))


def mul(a, b):
    if a[0] == 'n' or b[0] == 'n':
        return NAN
    s = a[1] ^ b[1]
    if a[0] == 'i' or b[0] == 'i':
        if (a[0] == 'f' and a[2] == 0) or (b[0] == 'f' and b[2] == 0):
            return NAN
        return ('i', s)
    return ('f', s, a[2] * b[2])


def div(a, b):
    if a[0] == 'n' or b[0] == 'n':
        return NAN
    s = a[1] ^ b[1]
    if a[0] == 'i':
        return NAN if b[0] == 'i' else ('i', s)
    if b[0] == 'i':
        return ('f', s, Fraction(0))
    if b[2] == 0:
        return NAN if a[2] == 0 else ('i', s)
    return ('f', s, a[2] / b[2])


def rel(a, b):
    """-1 less, 0 equal, 1 greater, None unordered (a relative to b)"""
    if a[0] == 'n' or b[0] == 'n':
        return None
    def key(v):
        if v[0] == 'i':
            return (2 if not v[1] else -2, 0)
        return (0, val(v))
    ka, kb = key(a), key(b)
    if ka[0] != kb[0] and (ka[0] in (2, -2) or kb[0] in (2, -2)):
        return -1 if ka[0] < kb[0] else 1 if ka[0] > kb[0] else 0
    if ka[0] == kb[0] and ka[0] != 0:
        return 0
    return (ka[1] > kb[1]) - (ka[1] < kb[1])


def d2b(sgn, exp, digits):
    """decimal record -> value"""
    if not digits or not digits.isdigit() or int(digits) == 0:
        return ('f', sgn & 1, Fraction(0))
    return ('f', sgn & 1, Fraction(int(digits)) * Fraction(10) ** exp)

"""Classic Mac resource-fork reader (Inside Macintosh: More Toolbox, ch. 1).

Reads the raw fork (an ExtFS `.rsrc/<name>` sidecar or any extracted fork) and
lists the map in on-disk order, which a byte-identical rebuild has to
reproduce: type-list order, reference-list order, name-list order, attributes
and data offsets.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass

@dataclass
class Resource:
    type: str
    id: int
    name: str | None
    attrs: int
    data_offset: int
    data: bytes
    type_order: int
    ref_order: int
    name_offset: int


@dataclass
class Fork:
    raw: bytes
    data_start: int
    map_start: int
    data_len: int
    map_len: int
    file_attrs: int
    type_list_off: int
    name_list_off: int
    resources: list[Resource]


def type_text(b: bytes) -> str:
    return b.decode("mac-roman")


def load(path: str) -> Fork:
    raw = open(path, "rb").read()
    data_start, map_start, data_len, map_len = struct.unpack(">IIII", raw[:16])
    m = raw[map_start:map_start + map_len]
    file_attrs, type_list_off, name_list_off = struct.unpack(">HHH", m[22:28])
    tl = type_list_off
    ntypes = struct.unpack(">h", m[tl:tl + 2])[0] + 1
    resources: list[Resource] = []
    for t in range(ntypes):
        o = tl + 2 + t * 8
        rtype = type_text(m[o:o + 4])
        count, ref_off = struct.unpack(">hH", m[o + 4:o + 8])
        for r in range(count + 1):
            ro = tl + ref_off + r * 12
            rid, noff = struct.unpack(">hh", m[ro:ro + 4])
            attrs = m[ro + 4]
            doff = int.from_bytes(m[ro + 5:ro + 8], "big")
            name = None
            if noff != -1:
                no = name_list_off + noff
                name = m[no + 1:no + 1 + m[no]].decode("mac-roman")
            do = data_start + doff
            dlen = struct.unpack(">I", raw[do:do + 4])[0]
            resources.append(Resource(rtype, rid, name, attrs, doff,
                                      raw[do + 4:do + 4 + dlen], t, r, noff))
    return Fork(raw, data_start, map_start, data_len, map_len, file_attrs,
                type_list_off, name_list_off, resources)


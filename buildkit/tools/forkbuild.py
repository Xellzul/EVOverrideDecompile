"""Rebuild a resource fork from a layout (taken from the user's fork, rsrc.load) with replacement data
{(type, id): bytes}."""
import struct


def build(f, repl):
    raw = f.raw
    # data area: resources in data-offset order, each u32 len + data, as found (check gaps)
    order = sorted(f.resources, key=lambda r: r.data_offset)
    data = bytearray(); offs = {}
    for r in order:
        offs[id(r)] = len(data)
        d = repl.get((r.type, r.id), r.data)
        data += struct.pack('>I', len(d)) + d
    # names, in name-offset order
    named = sorted([r for r in f.resources if r.name is not None], key=lambda r: r.name_offset)
    names = bytearray(); noff = {}
    seen = {}
    for r in named:
        k = r.name_offset
        if k in seen: noff[id(r)] = seen[k]; continue
        seen[k] = len(names); noff[id(r)] = len(names)
        b = r.name.encode('mac-roman'); names += bytes([len(b)]) + b
    om = f.raw[f.map_start:f.map_start + f.map_len]
    otl = f.type_list_off
    oref = {}
    for t in range(len(set(r.type_order for r in f.resources))):
        cnt, roff = struct.unpack('>hH', om[otl+2+t*8+4:otl+2+t*8+8])
        for k in range(cnt+1): oref[(t, k)] = om[otl+roff+k*12+8:otl+roff+k*12+12]
    types = []
    for r in f.resources:
        if r.type not in [t for t, _ in types]: types.append((r.type, []))
    for r in sorted(f.resources, key=lambda r: (r.type_order, r.ref_order)):
        dict(types)[r.type].append(r)
    ntypes = len(types)
    tl = 2 + 8 * ntypes
    tlist = bytearray(struct.pack('>h', ntypes - 1)); refs = bytearray(); ro = tl
    for t, rs in types:
        tlist += t.encode('mac-roman') + struct.pack('>hH', len(rs) - 1, ro + len(refs))
        for r in rs:
            n = noff[id(r)] if r.name is not None else -1
            refs += struct.pack('>hh', r.id, n) + bytes([r.attrs]) + offs[id(r)].to_bytes(3, 'big') + oref[(r.type_order, r.ref_order)]
    mp = bytearray(f.raw[f.map_start:f.map_start + 28])
    tstart = 28
    name_list = tstart + len(tlist) + len(refs)
    mp[24:26] = struct.pack('>H', tstart); mp[26:28] = struct.pack('>H', name_list)
    mp[22:24] = struct.pack('>H', f.file_attrs)
    mp += tlist + refs + names
    # header
    hdr = bytearray(raw[:f.data_start])
    ds = f.data_start; ms = ds + len(data)
    hdr[0:16] = struct.pack('>IIII', ds, ms, len(data), len(mp))
    mp[0:16] = hdr[0:16]
    return bytes(hdr) + bytes(data) + bytes(mp)

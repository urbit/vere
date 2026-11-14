#!/usr/bin/env python3
"""stencils.py OBJECT HEADER: the stencils of u3nc's native code.

Reads the object nock-compile.c compiles to with U3NC_STENCIL (x86-64 ELF,
a section per function) and writes nock-stencils.h: the machine code of
each stencil, the function _nj_burn_{north,south}_OP of an opcode, and its
relocations, as the tables nock-compile-jit.c lowers programs from.
"""
import re
import struct
import sys

HOLES = {
    '_nj_s0': ('nj_slot', 0), '_nj_s1': ('nj_slot', 1), '_nj_s2': ('nj_slot', 2),
    '_nj_v0': ('nj_value', 0), '_nj_v1': ('nj_value', 1), '_nj_v2': ('nj_value', 2),
    '_nj_next': ('nj_next', 0), '_nj_jump': ('nj_jump', 0),
}
#  R_X86_64_64, _PC32, _PLT32, _32, _32S
TYPES = {1: 'nj_abs64', 2: 'nj_pc32', 4: 'nj_pc32', 10: 'nj_abs32', 11: 'nj_abs32s'}
DIRS = ['_nj_burn_north_', '_nj_burn_south_']
IDENT = re.compile(r'^[A-Za-z_][A-Za-z0-9_]*$')


def die(msg):
    sys.exit('stencils.py: ' + msg)


def main(obj, out):
    d = open(obj, 'rb').read()
    if d[:4] != b'\x7fELF' or d[4] != 2 or d[5] != 1:
        die('not a 64-bit little-endian ELF object')
    if struct.unpack_from('<H', d, 0x12)[0] != 62:
        die('not x86-64')

    shoff = struct.unpack_from('<Q', d, 0x28)[0]
    shentsize, shnum, shstrndx = struct.unpack_from('<HHH', d, 0x3A)

    def cstr(base, off):
        return d[base + off:d.index(b'\0', base + off)].decode()

    secs = []
    for i in range(shnum):
        (name, typ, flags, _, off, size, link, info, _, _) = \
            struct.unpack_from('<IIQQQQIIQQ', d, shoff + i * shentsize)
        secs.append(dict(name=name, type=typ, flags=flags, off=off,
                         size=size, link=link, info=info))
    for s in secs:
        s['name'] = cstr(secs[shstrndx]['off'], s['name'])

    symtab = next(s for s in secs if s['type'] == 2)
    stroff = secs[symtab['link']]['off']
    syms = []
    for i in range(symtab['size'] // 24):
        (name, info, _, shndx, value, size) = \
            struct.unpack_from('<IBBHQQ', d, symtab['off'] + i * 24)
        syms.append(dict(name=cstr(stroff, name), bind=info >> 4,
                         type=info & 15, shndx=shndx, value=value, size=size))

    relas = {}
    for s in secs:
        if s['type'] == 4:
            relas[s['info']] = [
                struct.unpack_from('<QQq', d, s['off'] + i * 24)
                for i in range(s['size'] // 24)]

    #  the function at the start of a section
    func_at = {}
    for y in syms:
        if y['type'] == 2 and y['value'] == 0 and y['shndx'] not in (0, 0xfff1):
            func_at[y['shndx']] = y

    ext, ext_idx = [], {}       #  external symbols, by name
    rod = bytearray()           #  constants
    rod_at = {}                 #  section index -> offset in rod

    def ext_of(name):
        if not IDENT.match(name):
            die('symbol %r is not a C identifier' % name)
        if name not in ext_idx:
            ext_idx[name] = len(ext)
            ext.append(name)
        return ext_idx[name]

    def rod_of(shndx):
        if shndx not in rod_at:
            s = secs[shndx]
            if shndx in relas and relas[shndx]:
                die('constants %s have relocations' % s['name'])
            while len(rod) % 16:
                rod.append(0)
            rod_at[shndx] = len(rod)
            rod.extend(d[s['off']:s['off'] + s['size']])
        return rod_at[shndx]

    def target(y, where):
        """(sym kind, index, extra addend, local function name or None)"""
        if y['name'] in HOLES:
            return HOLES[y['name']] + (0, None)
        if y['shndx'] == 0:
            return ('nj_ext', ext_of(y['name']), 0, None)
        if y['shndx'] >= 0xff00:
            die('%s: symbol %r in special section' % (where, y['name']))
        s = secs[y['shndx']]
        if s['flags'] & 4:                                  #  code
            if y['type'] == 3:                              #  section symbol
                if y['shndx'] not in func_at:
                    die('%s: no function at the start of %s' % (where, s['name']))
                y = func_at[y['shndx']]
            elif y['value'] != 0:
                die('%s: %r is not at the start of its section' % (where, y['name']))
            return ('nj_ext', ext_of(y['name']), 0,
                    y['name'] if y['bind'] == 0 else None)
        if (s['flags'] & 1) or s['type'] == 8:              #  writable, or bss
            if y['type'] == 3 or not IDENT.match(y['name']):
                die('%s: data %r of %s has no name to share it by'
                    % (where, y['name'], s['name']))
            return ('nj_ext', ext_of(y['name']), 0, None)
        #  constants: copied, unless they have a name the build can take
        if y['type'] != 3 and IDENT.match(y['name']) and y['shndx'] in relas:
            return ('nj_ext', ext_of(y['name']), 0, None)
        return ('nj_rod', rod_of(y['shndx']), y['value'], None)

    #  the local functions whose addresses the stencil object takes
    #  (_nj_keep_v): the compiler leaves their calling convention alone
    kept = set()
    for y in syms:
        if y['name'] == '_nj_keep_v':
            for (off, info, add) in relas.get(y['shndx'], []):
                if y['value'] <= off < y['value'] + y['size']:
                    z = syms[info >> 32]
                    if z['type'] == 3:
                        z = func_at.get(z['shndx'], z)
                    kept.add(z['name'])

    code = bytearray()
    rels = []
    stens = [dict(), dict()]
    unkept = set()

    for y in sorted(syms, key=lambda y: y['name']):
        dir_i = next((i for i, p in enumerate(DIRS) if y['name'].startswith(p)), None)
        if dir_i is None or y['type'] != 2 or y['shndx'] == 0:
            continue
        op = y['name'][len(DIRS[dir_i]):]
        s = secs[y['shndx']]
        if y['value'] != 0:
            die('%s is not alone in its section' % y['name'])
        body = d[s['off']:s['off'] + y['size']]
        mine = sorted(r for r in relas.get(y['shndx'], []) if r[0] < y['size'])
        tal = 0
        first = len(rels)
        for (off, info, add) in mine:
            z, typ = syms[info >> 32], info & 0xffffffff
            where = '%s+%#x' % (y['name'], off)
            if typ not in TYPES:
                die('%s: relocation type %d' % (where, typ))
            (kind, idx, more, local) = target(z, where)
            if local and local not in kept:
                unkept.add(local)
            if (kind == 'nj_next' and TYPES[typ] == 'nj_pc32' and add == -4
                    and off == len(body) - 4 and body[-5] == 0xe9):
                tal = 1
            rels.append((off, TYPES[typ], kind, idx, add + more))
        stens[dir_i][op] = (len(code), len(body), first, len(rels) - first, tal)
        code.extend(body)

    if unkept:
        die('stencils call local functions the object does not keep: add to '
            '_nj_keep_v in nock-compile.c: ' + ', '.join(sorted(unkept)))
    if not stens[0] or set(stens[0]) != set(stens[1]):
        die('no stencils, or north and south differ')
    if len(code) >= 1 << 32:
        die('too much code')

    def table(name, data):
        rows = [', '.join(str(b) for b in data[i:i + 20])
                for i in range(0, len(data), 20)]
        return ('static const c3_y %s[] = {\n  %s\n};\n'
                % (name, ',\n  '.join(rows) if rows else '0'))

    w = []
    w.append('//  generated by jit/stencils.py: %d stencils per road direction, '
             '%d bytes\n//\n' % (len(stens[0]), len(code)))
    w.append(table('_nj_cod_y', code))
    w.append(table('_nj_rod_y', rod))
    w.append('static void* const _nj_ext_v[] = {\n'
             + ''.join('  (void*)&%s,\n' % n for n in ext) + '};\n')
    w.append('static const nj_rel _nj_rel_u[] = {\n'
             + ''.join('  { %d, %s, %s, %d, %d },\n' % r for r in rels) + '};\n')
    w.append('static const nj_sten _nj_sten_u[2][nj_last] = {\n')
    for dir_i in (0, 1):
        w.append('  {\n')
        for op in sorted(stens[dir_i]):
            w.append('    [%s] = { %d, %d, %d, %d, %d, "%s" },\n'
                     % ((op,) + stens[dir_i][op] + (op,)))
        w.append('  },\n')
    w.append('};\n')
    open(out, 'w').write(''.join(w))


if __name__ == '__main__':
    if len(sys.argv) != 3:
        die('usage: stencils.py OBJECT HEADER')
    main(sys.argv[1], sys.argv[2])

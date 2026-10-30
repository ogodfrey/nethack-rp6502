#!/usr/bin/env python3
"""Write the initial bytes of one global object of an ELF file to a file.

    elfsym.py ELF SYMBOL OUT

The build uses this after the link to make the XRAM image of a table, so the
image holds the same pointers as the program it is packaged with. The symbol
must have initial data (not .bss) in a loaded segment.
"""
import struct
import sys


def fail(msg):
    sys.exit('elfsym.py: ' + msg)


def read_symbol(data, name):
    if data[:4] != b'\x7fELF':
        fail('not an ELF file')
    is64 = data[4] == 2
    e = '<' if data[5] == 1 else '>'
    if is64:
        e_phoff, e_shoff = struct.unpack_from(e + 'QQ', data, 0x20)
        e_phentsize, e_phnum, e_shentsize, e_shnum = struct.unpack_from(
            e + 'HHHH', data, 0x36)
        ph_fmt = e + 'IIQQQQQQ'
        sh_fmt = e + 'IIQQQQIIQQ'
        sym_fmt = e + 'IBBHQQ'
    else:
        e_phoff, e_shoff = struct.unpack_from(e + 'II', data, 0x1c)
        e_phentsize, e_phnum, e_shentsize, e_shnum = struct.unpack_from(
            e + 'HHHH', data, 0x2a)
        ph_fmt = e + 'IIIIIIII'
        sh_fmt = e + 'IIIIIIIIII'
        sym_fmt = e + 'IIIBBH'

    segments = []
    for i in range(e_phnum):
        fields = struct.unpack_from(ph_fmt, data, e_phoff + i * e_phentsize)
        if is64:
            p_type, _, p_offset, p_vaddr, _, p_filesz = fields[:6]
        else:
            p_type, p_offset, p_vaddr, _, p_filesz = fields[:5]
        if p_type == 1:  # PT_LOAD
            segments.append((p_offset, p_vaddr, p_filesz))

    sections = []
    for i in range(e_shnum):
        fields = struct.unpack_from(sh_fmt, data, e_shoff + i * e_shentsize)
        sections.append(fields)

    for sh in sections:
        sh_type, sh_link = sh[1], sh[6]
        if sh_type != 2:  # SHT_SYMTAB
            continue
        sh_offset, sh_size, sh_entsize = sh[4], sh[5], sh[9]
        strtab = sections[sh_link]
        str_off = strtab[4]
        for off in range(sh_offset, sh_offset + sh_size, sh_entsize):
            st_name, st_value, st_size = None, None, None
            if is64:
                st_name, _, _, _, st_value, st_size = struct.unpack_from(
                    sym_fmt, data, off)
            else:
                st_name, st_value, st_size, _, _, _ = struct.unpack_from(
                    sym_fmt, data, off)
            start = str_off + st_name
            end = data.index(b'\0', start)
            if data[start:end].decode() != name:
                continue
            for p_offset, p_vaddr, p_filesz in segments:
                if p_vaddr <= st_value and st_value + st_size <= p_vaddr + p_filesz:
                    file_off = p_offset + (st_value - p_vaddr)
                    return data[file_off:file_off + st_size]
            fail('symbol %s has no initial data' % name)
    fail('symbol %s not found' % name)


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    with open(sys.argv[1], 'rb') as f:
        data = f.read()
    image = read_symbol(data, sys.argv[2])
    with open(sys.argv[3], 'wb') as f:
        f.write(image)


if __name__ == '__main__':
    main()

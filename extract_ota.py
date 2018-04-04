#!/usr/bin/env python3
import argparse
import base64
import json
import os
import sys
import re

from elftools.elf import elffile

mswindows = (sys.platform == "win32")

FLASH_OTA_BASE=0xd000
FLASH_OTA_MAX_LEN=0x2000
SRAM_OTA_BASE=0x20000000
SRAM_OTA_MAX_LEN=0x1000

class LinkerEntry(object):
    def __init__(self, object_file, section, bytes_=None):
        self.object_file = object_file
        self.section = section
        self.bytes = bytes_

    def is_resolved(self):
        return self.bytes != None

    def __str__(self):
        num_bytes = 0 if not self.bytes else len(self.bytes)
        res = 'resolved' if self.is_resolved() else 'unresolved'
        return ('LinkerEntry ({res}): object file = {obj}, section = {sec}'
                ', num bytes = {num_bytes}'.format(
                res=res, obj=self.object_file, sec=self.section,
                num_bytes=num_bytes))

def _range_contains_range(r1_start, r1_len, r2_start, r2_len):
    return (
        r1_start >= r2_start and
        (r1_start + r1_len) <= (r2_start + r2_len)
    )


def _seg_in_range(seg, r_start, r_len):
    return _range_contains_range(
        seg.header.p_vaddr,
        seg.header.p_memsz,
        r_start,
        r_len,
    )


def _seg_addr(seg):
    return seg.header.p_vaddr

def _find_entrypoint(obj):
    """Returns addresses of entry points in the ELF.

    Note that on Windows the generated ELF does not use unicode strings.
    """

    entrypoint = None
    symtab_name = '.symtab' if mswindows else b'.symtab'
    symtab = obj.get_section_by_name(symtab_name)
    for sym in symtab.iter_symbols():
        decname = sym.name if mswindows else sym.name.decode('utf8')
        if decname.startswith('__ota_entrypoint_'):
            entrypoint = decname[len('__ota_entrypoint_'):]

    if entrypoint is None:
        raise RuntimeError('No entrypoint found')

    entrypoint = entrypoint if mswindows else entrypoint.encode('utf8')
    for sym in symtab.iter_symbols():
        if sym.name == entrypoint:
            return sym.entry.st_value

    raise RuntimeError(
        "Could not find entrypoint symbol {0}".format(entrypoint)
    )

def filter_segments(elf, segments):
    """Filters out segments that don't correspond to .ota sections.

       e.g. .ota.* (e.g. .data:ti_sysbios_*).
    """
    seg_to_sect = {seg:[] for seg in segments}
    sections = (s for s in elf.iter_sections())
    for sec in sections:
        for seg in segments:
            if seg.section_in_segment(sec):
                seg_to_sect[seg].append(sec)

    fltr_segs = []
    for seg, sects in seg_to_sect.items():
        should_add = False
        for sec in sects:
            if sec.name in ('.ota.text', '.ota.data'):
                should_add = True
                break
        if should_add:
            fltr_segs.append(seg)

    return tuple(fltr_segs)

def extract_ota_code(params, binary_path):
    """Extracts text segment and data segment matadata.

    Data segment is compressed and the actual bytes returned are
    just placeholders that should be replaced with the app object file
    raw data.
    """
    def seg_in_ota_flash(seg):
        return _seg_in_range(seg, params.ota_flash_addr, params.ota_flash_len)

    def seg_in_ota_sram(seg):
        return _seg_in_range(seg, params.ota_sram_addr, params.ota_sram_len)

    def seg_in_ota(seg):
        return seg_in_ota_flash(seg) or seg_in_ota_sram(seg)

    with open(binary_path, 'rb') as f:
        obj = elffile.ELFFile(f)
        segments = tuple(
            sorted(
                (s for s in obj.iter_segments() if seg_in_ota(s)),
                key=_seg_addr,
            ),
        )
        segments = filter_segments(obj, segments)
        entrypoint = _find_entrypoint(obj) - params.ota_flash_addr

        #data = b''
        data = bytearray()
        data_offset = params.ota_flash_addr
        loads = []

        for seg in sorted(segments, key=lambda x: x.header.p_vaddr):
            if seg_in_ota_flash(seg):
                skip = seg.header.p_vaddr - data_offset
                data += (b'\x00' * skip)
                data_offset += skip
                seg_data = seg.data()
                data += seg_data
                data_offset += len(seg_data)
            else:
                loads.append(
                    {
                        'offset': data_offset - params.ota_flash_addr,
                        'len': seg.header.p_memsz,
                        'dest': seg.header.p_vaddr,
                    }
                )
                f.seek(seg.header.p_offset, os.SEEK_SET)
                seg_data = bytes(seg.header.p_memsz)
                data += seg_data
                data_offset += seg.header.p_memsz

    return loads, entrypoint, data

def extract_ota_data(binary_path, entries):
    with open(binary_path, 'rb') as f:
        elf = elffile.ELFFile(f)
        for entry in entries:
            if entry.is_resolved():
                continue

            sect = elf.get_section_by_name(entry.section)
            if sect:
                entry.bytes = sect.data()

def create_entries(text_entries):
    entries = []

    # Skip header.
    for text_entry in text_entries[1:]:
        entry = re.sub(r'\s+', ' ', text_entry.strip()).split(' ')
        size = int(entry[1], 16)
        if entry[-1] == '--HOLE--':
            entries.append(LinkerEntry(None, None, b'0' * size))
        else:
            object_file = entry[2]
            section = entry[3].strip('()')
            entries.append(LinkerEntry(object_file, section))

    return entries

def read_linker_map(path):
    OTA_DATA_REGEX = r'^.ota.data\s+(.+?)^$'
    with open(path) as f:
        data = f.read()
    m = re.search(OTA_DATA_REGEX, data, re.DOTALL | re.MULTILINE)
    if not m:
        raise RuntimeError("failed to find .ota.data mapping in linker file.")
    text_entries = m.group(1).splitlines()
    assert len(text_entries) > 1

    entries = create_entries(text_entries)
    return entries


def get_linker_map_path(out_file):
    pre, suf = os.path.splitext(out_file)
    map_file = pre + '.map'
    return map_file

def verify_resolved_entries(entries):
    unresolved = [e for e in entries if not e.is_resolved()]
    if unresolved:
        msg = 'Unresolved entries:'
        for entry in unresolved:
            msg += str(entry) + '\n'
        raise RuntimeError(msg)

def patch_data(loads, entries, data):
    assert len(loads) == 1, "we should have only one .ota.data entry."

    initial_offset = loads[0]['offset']
    offset = initial_offset
    len_ = loads[0]['len']
    for entry in entries:
        assert entry.is_resolved(), ("by this time all entries should've been read."
                                     "shame on you.")
        data[offset:offset + len(entry.bytes)] = entry.bytes
        offset += len(entry.bytes)

    if initial_offset + len_ != offset:
        raise RuntimeError('not all data bytes were patched.')

    return data

def extract_ota(params):
    """Extracts data and meta-data information from the ELF.

    Given a list of ELF files, (params.binary_paths), returns a JSON
    dictionary with the following kv pairs:
        size       - total size of the blob
        loads      - one or more chunks from .ota.data
                     (offset = offset for payload within the `data` blob
                      len = #of bytes in memory for the segment,
                      dest = load base address)
        entrypoint - offset of entrypoint (relative to flash base)
        data       - blob of code + data

    """
    out_file = list(b for b in params.binary_paths if b.endswith('.out'))[0]
    map_file = get_linker_map_path(out_file)
    if not os.path.exists(map_file):
        raise RuntimeError("linker map file doesn't exist: %s" % map_file)
    entries = read_linker_map(map_file)

    # Make sure .out file is always first.
    binary_paths = [out_file] + list(set(params.binary_paths) - set([out_file]))

    for binary_path in binary_paths:
        if binary_path.endswith('.out'):
            loads, entrypoint, data = extract_ota_code(params, binary_path)
        else:
            assert binary_path.endswith('.obj'), 'r u insane?'
            extract_ota_data(binary_path, entries)

    verify_resolved_entries(entries)
    data = patch_data(loads, entries, data)

    return {
        'size': len(data),
        'loads': loads,
        'entrypoint': entrypoint,
        'data': data,
    }

def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        'binary_paths',
        nargs='+',
        type=str,
        help='Path(s) to the binaries containing the code and data to be'
             'extracted.',
    )
    parser.add_argument(
        '--ota-flash-addr',
        type=int,
        default=FLASH_OTA_BASE,
        help='Start location OTA app in the flash',
        required=False,
    )
    parser.add_argument(
        '--ota-flash-len',
        type=int,
        default=FLASH_OTA_MAX_LEN,
        help='Max length of OTA app in the flash',
        required=False,
    )
    parser.add_argument(
        '--ota-sram-addr',
        type=int,
        default=SRAM_OTA_BASE,
        help='Start location OTA app in the SRAM',
        required=False,
    )
    parser.add_argument(
        '--ota-sram-len',
        type=int,
        default=SRAM_OTA_MAX_LEN,
        help='Max length of OTA app in the SRAM',
        required=False,
    )
    opts = parser.parse_args()
    return opts

def main():
    opts = parse_args()
    res = extract_ota(opts)
    res['data'] = ''.join(
        (
            '{0:02x}'.format(b) for b in res['data']
        ),
    )
    print(json.dumps(res, indent=4))
    # Remove the comment to see the raw loads data.
    """offset = res['loads'][0]['offset']
    len_ = res['loads'][0]['len']
    print(res['data'][offset * 2:(offset + len_) * 2])"""

if __name__ == '__main__':
    main()

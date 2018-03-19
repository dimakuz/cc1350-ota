#!/usr/bin/env python3
import argparse
import base64
import json
import os

from elftools.elf import elffile


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
    entrypoint = None
    symtab = obj.get_section_by_name(b'.symtab')
    for sym in symtab.iter_symbols():
        decname = sym.name.decode('utf8')
        if decname.startswith('__ota_entrypoint_'):
            entrypoint = decname[len('__ota_entrypoint_'):]

    if entrypoint is None:
        raise RuntimeError('No entrypoint found')

    entrypoint = entrypoint.encode('utf8')
    for sym in symtab.iter_symbols():
        if sym.name == entrypoint:
            return sym.entry.st_value

    raise RuntimeError(
        "Could not find entrypoint symbol {0}".format(entrypoint)
    )


def extract_ota(params):
    def seg_in_ota_flash(seg):
        return _seg_in_range(seg, params.ota_flash_addr, params.ota_flash_len)

    def seg_in_ota_sram(seg):
        return _seg_in_range(seg, params.ota_sram_addr, params.ota_sram_len)

    def seg_in_ota(seg):
        return seg_in_ota_flash(seg) or seg_in_ota_sram(seg)

    with open(params.binary_path, 'rb') as f:
        obj = elffile.ELFFile(f)
        segments = tuple(
            sorted(
                (s for s in obj.iter_segments() if seg_in_ota(s)),
                key=_seg_addr,
            ),
        )
        entrypoint = _find_entrypoint(obj) - params.ota_flash_addr

        data = b''
        data_offset = params.ota_flash_addr
        loads = []

        for seg in segments:
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
                seg_data = f.read(seg.header.p_memsz)
                data += seg_data
                data_offset += seg.header.p_memsz
    return {
        'size': len(data),
        'loads': loads,
        'entrypoint': entrypoint,
        'data': data,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        'binary_path',
        type=str,
        help='Path to binary containing the code to be extracted',
    )
    parser.add_argument(
        '--ota-flash-addr',
        type=int,
        default=0x17000,
        help='Start location OTA app in the flash',
        required=False,
    )
    parser.add_argument(
        '--ota-flash-len',
        type=int,
        default=0x4000,
        help='Max length of OTA app in the flash',
        required=False,
    )
    parser.add_argument(
        '--ota-sram-addr',
        type=int,
        default=0x20000000,
        help='Start location OTA app in the SRAM',
        required=False,
    )
    parser.add_argument(
        '--ota-sram-len',
        type=int,
        default=0x1000,
        help='Max length of OTA app in the SRAM',
        required=False,
    )
    opts = parser.parse_args()
    res = extract_ota(opts)
    res['data'] = ''.join(
        (
            '{0:02x}'.format(b) for b in res['data']
        ),
    )
    print(json.dumps(res, indent=4))

if __name__ == '__main__':
    main()

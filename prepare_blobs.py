#!/usr/bin/python3
import json
import math
import os
import sys
import shutil
import struct

_DEST_DIR = './ota_blobs'
_CHUNK_SIZE = 80
_CHUNK_OVERHEAD = 12
_CHUNK_PAYLOAD_SIZE = _CHUNK_SIZE - _CHUNK_OVERHEAD
OTA_MAGIC = 0xdabad000


def create_chunk(total_size, cur_chunk, num_chunks, csum, chunk_len, payload):
    return to_hex(
        struct.pack(
            '<LHBBHH',
            OTA_MAGIC,
            total_size,
            cur_chunk,
            num_chunks,
            csum,
            chunk_len,
        ),
    ) + payload


def to_hex(x):
    return ''.join(
        (
            '{0:02x}'.format(b) for b in x
        ),
    )


def iter_chunks(data):
    while data:
        chunk = data[:_CHUNK_PAYLOAD_SIZE * 2]
        data = data[_CHUNK_PAYLOAD_SIZE * 2:]
        yield chunk


def chunk_csum(chunk):
    return 0


def dump_load(dest, off, size):
    return struct.pack('<LHH', dest, off, size)


def dump_metadata(ota):
    res = struct.pack('<HH', ota['entrypoint'], int(len(ota['data']) / 2))
    for i in range(3):
        try:
            l = ota['loads'][i]
            load = (l['dest'], l['offset'], l['len'])
        except IndexError:
            load = (0, 0, 0)
        res += dump_load(*load)
    return to_hex(res)


def main():
    with open(sys.argv[1]) as f:
        ota = json.load(f)

    data = dump_metadata(ota) + ota['data']
    total_size = int(len(data) / 2)

    num_chunks = int(math.ceil(total_size / _CHUNK_PAYLOAD_SIZE))

    if os.path.isdir(_DEST_DIR):
        shutil.rmtree(_DEST_DIR)

    os.mkdir(_DEST_DIR)
    os.chdir(_DEST_DIR)

    for i, chunk in enumerate(iter_chunks(data)):
        res = create_chunk(
            total_size,
            i,
            num_chunks,
            chunk_csum(chunk),
            int(len(chunk) / 2),
            chunk,
        )
        with open('ota.chunk.{0}'.format(i), 'w') as f:
            f.write(res)

if __name__ == '__main__':
    main()

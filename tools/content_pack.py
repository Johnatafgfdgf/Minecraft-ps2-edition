"""Private streaming pack writer. Original inputs stay outside the public tree."""
import pathlib
import struct
import zlib
import shutil
from minecraft_reference import private_path
from import_minecraft_reference import safe_member


def write_pack(destination, entries, kind):
    destination = private_path(destination)
    if kind not in (1, 2):
        raise ValueError('Pack kind must be assets=1 or gameplay data=2')
    ordered = sorted(entries.items())
    records, payload_bytes = [], 0
    for name, payload in ordered:
        safe_member(name)
        encoded = name.encode('utf-8')
        if not encoded or len(encoded) > 65535:
            raise ValueError('Invalid pack path length')
        if isinstance(payload, pathlib.Path):
            crc, size = 0, payload.stat().st_size
            with payload.open('rb') as source:
                while chunk := source.read(1024 * 1024):
                    crc = zlib.crc32(chunk, crc)
        else:
            crc, size = zlib.crc32(payload), len(payload)
        records.append(struct.pack('<HHIQQ', len(encoded), 0, crc, payload_bytes, size) + encoded)
        payload_bytes += size
    index = b''.join(records)
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_suffix(destination.suffix + '.tmp')
    try:
        with temporary.open('wb') as out:
            out.write(struct.pack('<4sIIIQQ', b'MCPK', 1, kind, len(records), len(index), payload_bytes))
            out.write(index)
            for _, payload in ordered:
                if isinstance(payload, pathlib.Path):
                    with payload.open('rb') as source:
                        shutil.copyfileobj(source, out, length=1024*1024)
                else:
                    out.write(payload)
        temporary.replace(destination)
    finally:
        temporary.unlink(missing_ok=True)
    return {'entries': len(records), 'payload_bytes': payload_bytes, 'index_bytes': len(index), 'file_bytes': destination.stat().st_size}

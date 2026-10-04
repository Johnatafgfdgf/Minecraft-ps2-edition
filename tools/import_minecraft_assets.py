#!/usr/bin/env python3
"""Import a user's legitimate vanilla 1.21.1 client/assets into an ignored personal pack."""
import argparse
import hashlib
import io
import json
import pathlib
import struct
import zipfile
from minecraft_reference import LOCK, PRIVATE, digest, private_path
from content_pack import write_pack
from import_minecraft_reference import safe_member


def png_to_gs_texture(png):
    from PIL import Image
    with Image.open(io.BytesIO(png)) as image:
        rgba = bytearray(image.convert('RGBA').tobytes())
        width, height = image.size
    # GS full alpha is 0x80. Keep every pixel/frame of animation strips.
    for index in range(3, len(rgba), 4):
        rgba[index] = (rgba[index] + 1) // 2
    return struct.pack('<4s5I', b'MCPT', 1, width, height, 4, 1) + rgba


def import_assets(client, output, assets_root=None):
    output = private_path(output)
    if digest(client) != LOCK['client_sha1']:
        raise ValueError('Supply the legitimate, unmodified Java 1.21.1 client JAR; hash mismatch.')
    entries, converted, texture_bytes = {}, 0, 0
    with zipfile.ZipFile(client) as jar:
        version = json.loads(jar.read('version.json'))
        if version['id'] != '1.21.1':
            raise ValueError('Wrong client version')
        for name in jar.namelist():
            if not name.startswith('assets/') or name.endswith('/'):
                continue
            safe_member(name)
            contents = jar.read(name)
            entries[name] = contents
            if name.endswith('.png'):
                texture = png_to_gs_texture(contents)
                entries['ps2_textures/' + name.removeprefix('assets/') + '.mcpt'] = texture
                converted += 1
                texture_bytes += len(texture)
    external = 0
    if assets_root:
        root = pathlib.Path(assets_root).resolve()
        index_path = root / 'indexes' / (LOCK['asset_index_id'] + '.json')
        if digest(index_path) != LOCK['asset_index_sha1']:
            raise ValueError('Wrong/modified 1.21.1 asset index; do not guess sound metadata.')
        index = json.loads(index_path.read_text())
        for name, record in index['objects'].items():
            safe_member(name)
            sha = record['hash']
            if len(sha) != 40 or any(c not in '0123456789abcdef' for c in sha):
                raise ValueError('Invalid resource object hash')
            source = root / 'objects' / sha[:2] / sha
            if digest(source) != sha or source.stat().st_size != record['size']:
                raise ValueError(f'Missing/corrupt personal resource object: {name}')
            entries['assets/' + name] = source
            external += 1
    output.mkdir(parents=True, exist_ok=True)
    stats = write_pack(output / 'assets.pack', entries, kind=1)
    report = {
        'version': '1.21.1', 'client_sha1': LOCK['client_sha1'], 'pack': stats,
        'gs_textures': converted, 'gs_texture_bytes': texture_bytes, 'external_objects': external,
        'external_assets_supplied': bool(assets_root),
        'complete_installation_assets': bool(assets_root),
        'preserved': 'Original resource entries, model/animation metadata and all PNG pixels/strip frames.',
        'runtime_status': 'Pack import only; renderer/audio decoders are still under development.',
        'pack_sha256': digest(output / 'assets.pack', 'sha256'),
    }
    (output / 'asset_provenance.json').write_text(json.dumps(report, indent=2) + '\n')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client', type=pathlib.Path, required=True)
    parser.add_argument('--assets-root', type=pathlib.Path, help='The assets directory of the same legitimate installation (indexes/objects).')
    parser.add_argument('--output', type=pathlib.Path, default=PRIVATE / 'assets')
    args = parser.parse_args()
    try:
        report = import_assets(args.client, args.output, args.assets_root)
    except (ValueError, OSError, zipfile.BadZipFile, ImportError) as error:
        parser.exit(1, f'Asset import failed: {error}\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()

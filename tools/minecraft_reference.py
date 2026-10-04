"""Local reference helpers. No Minecraft implementation or content is embedded."""
import hashlib
import json
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parents[1]
PRIVATE = ROOT / '.local'
REFERENCE = PRIVATE / 'reference'
LOCK = {
    'version': '1.21.1',
    'server_sha1': '59353fb40c36d304f2035d51e7d6e6baa98dc05c',
    'server_mappings_sha1': '03f8985492bda0afc0898465341eb0acef35f570',
    'client_sha1': '30c73b1c5da787909b2f73340419fdf13b9def88',
    'asset_index_sha1': '9b16298b1dc0697878cec88bb2d96168f5239e4f',
    'asset_index_id': '17',
    'server_url': 'https://piston-data.mojang.com/v1/objects/59353fb40c36d304f2035d51e7d6e6baa98dc05c/server.jar',
    'mappings_url': 'https://piston-data.mojang.com/v1/objects/03f8985492bda0afc0898465341eb0acef35f570/server.txt',
}


def digest(path, algorithm='sha1'):
    h = hashlib.new(algorithm)
    with pathlib.Path(path).open('rb') as f:
        while chunk := f.read(1024 * 1024):
            h.update(chunk)
    return h.hexdigest()


def private_path(path):
    result = pathlib.Path(path).resolve()
    if not result.is_relative_to(PRIVATE) or result == PRIVATE:
        raise ValueError(f'Generated Minecraft material must remain below {PRIVATE}')
    return result


def parse_mappings(text):
    classes = {}
    current = None
    for line in text.splitlines():
        if not line or line.startswith('#'):
            continue
        if not line.startswith(' '):
            name, symbol = line.removesuffix(':').split(' -> ', 1)
            current = {'symbol': symbol, 'members': {}}
            classes[name] = current
        elif ' -> ' in line and current is not None:
            signature, symbol = line.strip().split(' -> ', 1)
            signature = re.sub(r'^\d+:\d+:', '', signature)
            signature = re.sub(r':\d+:\d+$', '', signature)
            current['members'][signature] = symbol
    return classes


def load_reference(path=REFERENCE):
    path = private_path(path)
    manifest = json.loads((path / 'manifest.json').read_text())
    if manifest.get('version') != LOCK['version']:
        raise ValueError('Reference version must be exactly 1.21.1')
    for key in ('server_sha1', 'server_mappings_sha1'):
        if manifest.get(key) != LOCK[key]:
            raise ValueError(f'Reference provenance mismatch: {key}')
    for name, expected in manifest['file_sha256'].items():
        target = (path / name).resolve()
        if not target.is_relative_to(path) or digest(target, 'sha256') != expected:
            raise ValueError(f'Reference input changed: {name}')
    return path, manifest, json.loads((path / 'symbols.json').read_text())

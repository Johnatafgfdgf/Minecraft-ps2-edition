#!/usr/bin/env python3
"""Verify/import a legitimate 1.21.1 server and mappings into ignored local storage."""
import argparse
import hashlib
import json
import pathlib
import shutil
import tempfile
import urllib.request
import zipfile
from minecraft_reference import LOCK, REFERENCE, digest, parse_mappings, private_path


def safe_member(name):
    path = pathlib.PurePosixPath(name)
    if path.is_absolute() or '..' in path.parts or '\\' in name:
        raise ValueError('Unsafe JAR entry')
    return path


def import_reference(server, mappings, output):
    output = private_path(output)
    if output.exists():
        raise ValueError('Reference destination already exists; choose a new .local subdirectory.')
    if digest(server) != LOCK['server_sha1'] or digest(mappings) != LOCK['server_mappings_sha1']:
        raise ValueError('Input hashes do not match the official 1.21.1 server/mappings.')
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='mcps2-import-', dir=output.parent) as temporary:
        stage = pathlib.Path(temporary) / 'verified'
        stage.mkdir()
        shutil.copyfile(server, stage / 'server.jar')
        shutil.copyfile(mappings, stage / 'server-mappings.txt')
        classpath = []
        with zipfile.ZipFile(server) as bundle:
            version_entries = bundle.read('META-INF/versions.list').decode().splitlines()
            if len(version_entries) != 1:
                raise ValueError('Unexpected server bundle')
            inner = None
            for family in ('versions', 'libraries'):
                entries = bundle.read(f'META-INF/{family}.list').decode().splitlines()
                for line in entries:
                    expected, identifier, name = line.split('\t')
                    safe_member(name)
                    contents = bundle.read(f'META-INF/{family}/{name}')
                    if hashlib.sha256(contents).hexdigest() != expected:
                        raise ValueError(f'Bundled file hash mismatch: {name}')
                    destination = stage / family / name
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    destination.write_bytes(contents)
                    classpath.append(str(destination.relative_to(stage)))
                    if family == 'versions':
                        if identifier != LOCK['version']:
                            raise ValueError('Wrong Minecraft version')
                        inner = destination
        with zipfile.ZipFile(inner) as archive:
            version = json.loads(archive.read('version.json'))
        if version['id'] != LOCK['version'] or version['world_version'] != 3955:
            raise ValueError('Wrong internal version/data version')
        symbols = parse_mappings(pathlib.Path(mappings).read_text())
        (stage / 'symbols.json').write_text(json.dumps(symbols, sort_keys=True))
        checksums = {str(p.relative_to(stage)): digest(p, 'sha256') for p in stage.rglob('*') if p.is_file()}
        manifest = {
            'version': LOCK['version'], 'world_version': version['world_version'],
            'server_sha1': LOCK['server_sha1'], 'server_mappings_sha1': LOCK['server_mappings_sha1'],
            'classpath': classpath, 'inner_jar': str(inner.relative_to(stage)),
            'file_sha256': checksums,
        }
        (stage / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
        shutil.move(str(stage), output)
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=pathlib.Path)
    parser.add_argument('--mappings', type=pathlib.Path)
    parser.add_argument('--download-official-server', action='store_true', help='Use the public Mojang server distribution; never a third-party decompilation.')
    parser.add_argument('--output', type=pathlib.Path, default=REFERENCE)
    args = parser.parse_args()
    try:
        if args.download_official_server:
            if args.server or args.mappings:
                parser.error('Choose local files or the official download option.')
            with tempfile.TemporaryDirectory(prefix='mcps2-official-') as tmp:
                files = []
                for url in (LOCK['server_url'], LOCK['mappings_url']):
                    target = pathlib.Path(tmp) / pathlib.PurePosixPath(url).name
                    with urllib.request.urlopen(url, timeout=60) as src, target.open('wb') as out:
                        shutil.copyfileobj(src, out)
                    files.append(target)
                manifest = import_reference(*files, args.output)
        else:
            if not args.server or not args.mappings:
                parser.error('Provide --server and --mappings, or --download-official-server.')
            manifest = import_reference(args.server, args.mappings, args.output)
    except (ValueError, OSError, zipfile.BadZipFile) as error:
        parser.exit(1, f'Import failed: {error}\n')
    print(f"Verified Minecraft {manifest['version']}; imported {len(manifest['classpath'])} local JARs.")


if __name__ == '__main__':
    main()

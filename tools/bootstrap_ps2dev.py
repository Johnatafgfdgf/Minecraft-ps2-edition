#!/usr/bin/env python3
"""Install a checksum-pinned ps2dev release without changing login scripts."""
import argparse
import hashlib
import json
import pathlib
import platform
import subprocess
import tempfile
import urllib.request


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prefix', type=pathlib.Path, default=pathlib.Path('.local/ps2dev'))
    args = parser.parse_args()
    if platform.system() != 'Linux' or platform.machine() not in ('x86_64', 'AMD64'):
        parser.error('This lock targets Linux x86_64; use a matching official ps2dev release.')
    prefix = args.prefix.resolve()
    if ' ' in str(prefix):
        parser.error('PS2DEV cannot contain spaces.')
    if prefix.exists() and any(prefix.iterdir()):
        parser.error('Choose an empty prefix; existing toolchains will not be overwritten.')
    lock = json.loads(pathlib.Path(__file__).with_name('toolchain.lock.json').read_text())
    with tempfile.TemporaryDirectory(prefix='mcps2-toolchain-') as tmp:
        archive = pathlib.Path(tmp) / 'ps2dev.tar.gz'
        digest = hashlib.sha256()
        with urllib.request.urlopen(lock['url'], timeout=60) as source, archive.open('wb') as out:
            while chunk := source.read(1024 * 1024):
                digest.update(chunk)
                out.write(chunk)
        if digest.hexdigest() != lock['sha256']:
            raise SystemExit('ps2dev SHA-256 mismatch; nothing was extracted.')
        prefix.mkdir(parents=True, exist_ok=True)
        subprocess.run(['tar', '-xzf', str(archive), '--strip-components=1', '-C', str(prefix)], check=True)
    print(f'export PS2DEV={prefix}')
    print('export PS2SDK=$PS2DEV/ps2sdk')
    print('export GSKIT=$PS2DEV/gsKit')
    print('export PATH=$PS2DEV/ee/bin:$PS2DEV/iop/bin:$PS2DEV/dvp/bin:$PS2SDK/bin:$PATH')


if __name__ == '__main__':
    main()

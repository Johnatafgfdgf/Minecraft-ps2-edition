#!/usr/bin/env python3
"""Reject proprietary reference trees and generated content before publication."""
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
BAD_PARTS = {'.local', 'decompiled', 'reference', 'oracle-output'}
BAD_SUFFIXES = {'.jar', '.class', '.irx', '.elf', '.png', '.ogg', '.nbt', '.mcpack', '.mcdg'}


def forbidden(path):
    p = pathlib.PurePosixPath(path)
    return bool(set(p.parts) & BAD_PARTS or p.suffix.lower() in BAD_SUFFIXES
                or path.startswith(('assets/imported/', 'data/generated/', 'build/', 'dist/'))
                or p.name in ('client.txt', 'server.txt', 'server-mappings.txt', 'symbols.json', 'blocks.json', 'registries.json'))


def main():
    tracked = subprocess.check_output(['git', 'ls-files', '-z'], cwd=ROOT).decode().split('\0')
    rejected = [p for p in tracked if p and forbidden(p)]
    if rejected:
        raise SystemExit('Public-tree audit failed:\n' + '\n'.join(rejected))
    print(f'Public-tree audit: {len([p for p in tracked if p])} tracked paths; no prohibited inputs.')


if __name__ == '__main__':
    main()

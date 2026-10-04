#!/usr/bin/env python3
"""Run the original data generator, then create private compact registries/data packs."""
import argparse
import hashlib
import json
import os
import pathlib
import subprocess
import zipfile
from minecraft_reference import ROOT, REFERENCE, PRIVATE, load_reference, private_path
from compile_block_states import compile_block_states
from content_pack import write_pack
from run_parity import java_tool


def transition_cases(blocks):
    result = []
    for name, block in sorted(blocks.items()):
        domains = block.get('properties', {})
        for state in block['states']:
            properties = sorted(state.get('properties', {}).items())
            if not properties:
                continue
            key, old = properties[state['id'] % len(properties)]
            choices = domains[key]
            value = choices[(choices.index(old) + 1) % len(choices)]
            result.append(f"state {state['id']} {key} {value}")
        default = next(s for s in block['states'] if s.get('default'))
        for key, choices in sorted(domains.items()):
            for value in choices:
                result.append(f"state {default['id']} {key} {value}")
        result.append(f"state {default['id']} absent_property invalid_value")
        if domains:
            result.append(f"state {default['id']} {sorted(domains)[0]} invalid_value")
    return result


def expected_dump(blocks, registries):
    states = []
    for name, block in blocks.items():
        block_id = registries['minecraft:block']['entries'][name]['protocol_id']
        for state in block['states']:
            fields = [str(state['id']), str(block_id), name, str(int(state.get('default', False)))]
            fields += [f'{key}={value}' for key, value in sorted(state.get('properties', {}).items())]
            states.append((state['id'], '\t'.join(fields)))
    return '\n'.join(line for _, line in sorted(states)) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference', type=pathlib.Path, default=REFERENCE)
    parser.add_argument('--output', type=pathlib.Path, default=PRIVATE / 'content')
    parser.add_argument('--verify-runner', type=pathlib.Path)
    args = parser.parse_args()
    reference, manifest, _ = load_reference(args.reference)
    output = private_path(args.output)
    reports = PRIVATE / 'reports'
    reports.mkdir(parents=True, exist_ok=True)
    cp = os.pathsep.join(str(reference / name) for name in manifest['classpath'])
    result = subprocess.run([java_tool('java'), '-cp', cp, 'net.minecraft.data.Main', '--reports', '--output', str(reports)],
                            cwd=reports, capture_output=True, text=True)
    if result.returncode:
        raise SystemExit('Original data generator failed:\n' + result.stderr)
    blocks = json.loads((reports / 'reports/blocks.json').read_text())
    registries = json.loads((reports / 'reports/registries.json').read_text())
    binary, stats = compile_block_states(blocks, registries)
    output.mkdir(parents=True, exist_ok=True)
    binary_path = output / 'block_states.bin'
    binary_path.write_bytes(binary)
    mappings = {name: {key: {'java_id': entry['protocol_id'], 'ps2_id': entry['protocol_id']}
               for key, entry in registry['entries'].items()} for name, registry in registries.items()}
    (output / 'registry_ids.json').write_text(json.dumps(mappings, sort_keys=True) + '\n')
    entries = {}
    with zipfile.ZipFile(reference / manifest['inner_jar']) as jar:
        for name in jar.namelist():
            if name.startswith('data/') and not name.endswith('/'):
                entries[name] = jar.read(name)
    data_stats = write_pack(output / 'world_data.pack', entries, kind=2)
    inputs = transition_cases(blocks)
    (output / 'state_cases.txt').write_text('\n'.join(inputs) + '\n')
    expected = expected_dump(blocks, registries)
    verified = False
    if args.verify_runner:
        actual = subprocess.check_output([str(args.verify_runner.resolve()), str(binary_path), '--dump'], text=True)
        if actual != expected:
            raise SystemExit('Native registry differs from original reports; generation is not approved.')
        verified = True
    report = {
        'version': manifest['version'], 'server_sha1': manifest['server_sha1'], 'data_version': 3955,
        'block_states': stats, 'registries': len(mappings), 'world_data_pack': data_stats,
        'state_transition_cases': len(inputs), 'native_report_roundtrip_verified': verified,
        'registry_sha256': hashlib.sha256(binary).hexdigest(),
        'original_report_sha256': hashlib.sha256((reports / 'reports/blocks.json').read_bytes()).hexdigest(),
        'scope': 'Content representation only; block mechanics and data interpreters remain pending.',
    }
    (output / 'provenance.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()

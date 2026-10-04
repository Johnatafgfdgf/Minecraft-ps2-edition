"""Convert locally generated original reports into an independent compact MCSR representation."""
import struct


def compile_block_states(blocks, registries):
    block_registry = registries['minecraft:block']['entries']
    if set(blocks) != set(block_registry):
        raise ValueError('Block report and original registry contain different blocks')
    ordered = sorted(blocks, key=lambda name: block_registry[name]['protocol_id'])
    if [block_registry[name]['protocol_id'] for name in ordered] != list(range(len(ordered))):
        raise ValueError('Block IDs must form a dense mapping; no silent reassignment.')
    count = sum(len(blocks[name]['states']) for name in ordered)
    states = [None] * count
    pairs = sorted({pair for name in ordered for state in blocks[name]['states'] for pair in state.get('properties', {}).items()})
    if len(pairs) > 65536:
        raise ValueError('Property dictionary exceeds this format; increase format capacity.')
    pair_ids = {pair: i for i, pair in enumerate(pairs)}
    string_data = bytearray()
    string_offsets = {}

    def intern(value):
        if '\0' in value:
            raise ValueError('NUL in resource/property name')
        if value not in string_offsets:
            string_offsets[value] = len(string_data)
            string_data.extend(value.encode('utf-8') + b'\0')
        return string_offsets[value]

    block_data = bytearray()
    state_refs = []
    for block_id, name in enumerate(ordered):
        source = sorted(blocks[name]['states'], key=lambda state: state['id'])
        ids = [s['id'] for s in source]
        if not ids or ids != list(range(ids[0], ids[0] + len(ids))):
            raise ValueError(f'State IDs for {name} are not contiguous')
        defaults = [s['id'] for s in source if s.get('default', False)]
        if len(defaults) != 1:
            raise ValueError(f'{name} must have one default state')
        block_data.extend(struct.pack('<4I', intern(name), defaults[0], ids[0], len(ids)))
        for state in source:
            state_id = state['id']
            if state_id < 0 or state_id >= count or states[state_id] is not None:
                raise ValueError('Invalid/duplicate original state ID')
            refs = [pair_ids[p] for p in sorted(state.get('properties', {}).items())]
            states[state_id] = struct.pack('<3I', block_id, len(state_refs), len(refs))
            state_refs.extend(refs)
    if any(state is None for state in states):
        raise ValueError('Missing original state IDs')
    pair_data = b''.join(struct.pack('<2I', intern(name), intern(value)) for name, value in pairs)
    reference_data = b''.join(struct.pack('<H', index) for index in state_refs)
    header = struct.pack('<4s7I', b'MCSR', 1, len(ordered), count, len(pairs), len(state_refs), len(string_data), 3955)
    result = bytes(header + block_data + b''.join(states) + pair_data + reference_data + string_data)
    return result, {'blocks': len(ordered), 'states': count, 'property_pairs': len(pairs), 'property_references': len(state_refs), 'bytes': len(result)}

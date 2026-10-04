import pathlib
import sys
import tempfile
import unittest
import json
import struct
import subprocess
import zlib
import io

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools'))
from minecraft_reference import parse_mappings, private_path, PRIVATE, ROOT
from import_minecraft_reference import safe_member, import_reference
from check_public_tree import forbidden
from compile_block_states import compile_block_states
from content_pack import write_pack
from import_minecraft_assets import png_to_gs_texture, import_assets


class ReferenceTests(unittest.TestCase):
    def test_mapping_overloads_and_line_numbers(self):
        parsed = parse_mappings('example.Sample -> abc:\n    1:9:int nextInt() -> a\n    10:12:int nextInt(int) -> b\n    long seed -> c\n')
        self.assertEqual(parsed['example.Sample']['symbol'], 'abc')
        self.assertEqual(parsed['example.Sample']['members']['int nextInt(int)'], 'b')
        self.assertEqual(parsed['example.Sample']['members']['long seed'], 'c')

    def test_generated_inputs_cannot_escape_private_storage(self):
        with self.assertRaises(ValueError):
            private_path(PRIVATE / '..' / 'data' / 'generated')
        with self.assertRaises(ValueError):
            private_path(PRIVATE)
        self.assertEqual(private_path(PRIVATE / 'reference'), PRIVATE / 'reference')
        for name in ('../x.jar', '/x.jar', 'a/../../x', 'a\\x.jar'):
            with self.assertRaises(ValueError):
                safe_member(name)

    def test_wrong_reference_rejected_before_output_is_created(self):
        with tempfile.TemporaryDirectory() as tmp:
            fake = pathlib.Path(tmp) / 'fake'
            fake.write_bytes(b'not Minecraft')
            destination = PRIVATE / 'test-rejected-reference'
            with self.assertRaises(ValueError):
                import_reference(fake, fake, destination)
            self.assertFalse(destination.exists())

    def test_public_audit_rejects_proprietary_and_binary_paths(self):
        for name in ('.local/reference/a.txt', 'foo/client.jar', 'build/ps2/MinecraftPS2.elf', 'data/generated/registry.bin', 'textures/stone.png', 'foo/blocks.json'):
            self.assertTrue(forbidden(name), name)
        self.assertFalse(forbidden('tools/import_minecraft_reference.py'))
        self.assertFalse(forbidden('docs/PORT_MAPPING.md'))


class ContentTests(unittest.TestCase):
    @staticmethod
    def synthetic_blocks():
        # Authored test content, not an extracted Minecraft dataset.
        return {'example:machine': {'properties': {'height': ['low', 'high']}, 'states': [
            {'id': 0, 'default': True, 'properties': {'height': 'low'}},
            {'id': 1, 'properties': {'height': 'high'}}]}}, {
            'minecraft:block': {'entries': {'example:machine': {'protocol_id': 0}}}}

    def test_compiler_rejects_missing_duplicate_and_sparse_ids(self):
        blocks, registries = self.synthetic_blocks()
        binary, stats = compile_block_states(blocks, registries)
        self.assertEqual(binary[:4], b'MCSR')
        self.assertEqual(stats['states'], 2)
        broken = json.loads(json.dumps(blocks))
        broken['example:machine']['states'][1]['id'] = 0
        with self.assertRaises(ValueError):
            compile_block_states(broken, registries)
        broken = json.loads(json.dumps(registries))
        broken['minecraft:block']['entries']['example:machine']['protocol_id'] = 3
        with self.assertRaises(ValueError):
            compile_block_states(blocks, broken)
        broken['minecraft:block']['entries']['example:missing'] = {'protocol_id': 1}
        with self.assertRaises(ValueError):
            compile_block_states(blocks, broken)

    @unittest.skipUnless((ROOT/'build/host/registry').is_file(), 'Build native registry runner with make test')
    def test_native_registry_transitions_and_corrupt_input_rejection(self):
        PRIVATE.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=PRIVATE, prefix='registry-test-') as tmp:
            binary, _ = compile_block_states(*self.synthetic_blocks())
            target = pathlib.Path(tmp)/'synthetic.bin'
            target.write_bytes(binary)
            runner = str(ROOT/'build/host/registry')
            result = subprocess.check_output([runner,str(target)], input='state 0 height high\nstate 1 height low\nstate 1 height high\nstate 0 height missing\nstate 0 missing value\n', text=True)
            self.assertEqual(result.splitlines(), ['R\t0\tstate 1','R\t1\tstate 0','R\t2\tstate 1','R\t3\tstate -1','R\t4\tstate -1'])
            for offset in (8,12,20,24,32,48):
                malformed = bytearray(binary)
                struct.pack_into('<I', malformed, offset, 0xffffffff)
                target.write_bytes(malformed)
                self.assertEqual(subprocess.run([runner,str(target)], capture_output=True).returncode, 3, offset)
            target.write_bytes(binary[:-1]+b'x')
            self.assertEqual(subprocess.run([runner,str(target)], capture_output=True).returncode, 3)

    def test_pack_offsets_crc_and_private_output(self):
        PRIVATE.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=PRIVATE, prefix='pack-test-') as tmp:
            tmp = pathlib.Path(tmp)
            source=tmp/'source'; source.write_bytes(b'large-stream-fixture')
            destination=tmp/'content.pack'
            info=write_pack(destination, {'data/example/a':b'abc','data/example/b':source}, kind=2)
            blob=destination.read_bytes()
            magic, version, kind, count, index_size, payload_size=struct.unpack_from('<4sIIIQQ',blob)
            self.assertEqual((magic,version,kind,count),(b'MCPK',1,2,2))
            self.assertEqual(info['file_bytes'],32+index_size+payload_size)
            cursor=32
            for expected_name,payload in [('data/example/a',b'abc'),('data/example/b',source.read_bytes())]:
                length,reserved,crc,offset,size=struct.unpack_from('<HHIQQ',blob,cursor)
                cursor+=24
                name=blob[cursor:cursor+length].decode();cursor+=length
                self.assertEqual(name,expected_name)
                self.assertEqual(reserved,0)
                self.assertEqual(blob[32+index_size+offset:32+index_size+offset+size],payload)
                self.assertEqual(crc,zlib.crc32(payload))
            with self.assertRaises(ValueError):
                write_pack(ROOT/'public.pack',{'data/example/a':b'abc'},2)
            with self.assertRaises(ValueError):
                write_pack(tmp/'bad.pack',{'../escape':b'abc'},2)

    def test_texture_conversion_preserves_strip_and_alpha(self):
        try:
            from PIL import Image
        except ImportError:
            self.skipTest('Install requirements-tools.txt for PNG conversion tests')
        image=Image.new('RGBA',(1,2))
        image.putpixel((0,0),(1,2,3,255));image.putpixel((0,1),(4,5,6,0))
        stream=io.BytesIO();image.save(stream,format='PNG')
        result=png_to_gs_texture(stream.getvalue())
        self.assertEqual(struct.unpack_from('<4s5I',result),(b'MCPT',1,1,2,4,1))
        self.assertEqual(result[24:],bytes([1,2,3,128,4,5,6,0]))

    def test_client_input_must_match_the_legitimate_reference(self):
        with tempfile.TemporaryDirectory() as tmp:
            fake=pathlib.Path(tmp)/'fake.jar';fake.write_bytes(b'not Minecraft')
            with self.assertRaises(ValueError):
                import_assets(fake,PRIVATE/'rejected-assets')
            self.assertFalse((PRIVATE/'rejected-assets').exists())


if __name__ == '__main__':
    unittest.main()

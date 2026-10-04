import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools'))
from minecraft_reference import parse_mappings, private_path, PRIVATE
from import_minecraft_reference import safe_member, import_reference
from check_public_tree import forbidden


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


if __name__ == '__main__':
    unittest.main()

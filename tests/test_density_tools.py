import math
import pathlib
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from compile_density_graph import DensityCompiler,UnsupportedDensity,NODE,HEADER


class DensityToolsTests(unittest.TestCase):
    def validate(self,payload,accepted):
        with tempfile.TemporaryDirectory() as temporary:
            path=pathlib.Path(temporary)/'authored.mcdg';path.write_bytes(payload)
            result=subprocess.run([str(ROOT/'build/host/density-data'),str(path)],capture_output=True)
            self.assertEqual(result.returncode,0 if accepted else 3,result.stderr)

    def recalculate(self,payload):
        struct.pack_into('<I',payload,36,zlib.crc32(payload[HEADER.size:])); return payload

    def test_named_reference_and_signed_zero_are_retained(self):
        compiler=DensityCompiler({'example:source':-0.0},{})
        root=compiler.compile({'type':'minecraft:mul','argument1':'example:source','argument2':2.0})
        self.assertEqual([n[0] for n in compiler.nodes],[0,31,0,3])
        pack=compiler.pack(root,False)
        self.assertEqual(pack[HEADER.size+24:HEADER.size+32],struct.pack('<d',-0.0))
        self.validate(pack,True)

    def test_missing_cycles_and_unsupported_nodes_fail_explicitly(self):
        with self.assertRaisesRegex(ValueError,'Cyclic'):
            DensityCompiler({'example:a':'example:b','example:b':'example:a'},{}).compile('example:a')
        with self.assertRaisesRegex(ValueError,'Missing density'):
            DensityCompiler({},{}).compile('example:missing')
        with self.assertRaisesRegex(UnsupportedDensity,'spline'):
            DensityCompiler({},{}).compile({'type':'minecraft:spline','spline':{}})
        with self.assertRaisesRegex(ValueError,'Missing noise'):
            DensityCompiler({},{}).compile({'type':'minecraft:shift','argument':'example:missing'})

    def test_reader_rejects_truncation_crc_future_references_and_domain(self):
        compiler=DensityCompiler({},{});root=compiler.compile({'type':'minecraft:abs','argument':-2.0})
        pack=compiler.pack(root,False)
        for cut in (0,4,39,len(pack)-1): self.validate(pack[:cut],False)
        corrupt=bytearray(pack);corrupt[-1]^=1;self.validate(corrupt,False)
        corrupt=bytearray(pack);struct.pack_into('<I',corrupt,8,0);self.validate(corrupt,False)
        corrupt=bytearray(pack);struct.pack_into('<I',corrupt,HEADER.size+NODE.size+4,root)
        self.validate(self.recalculate(corrupt),False)
        corrupt=bytearray(pack);corrupt[HEADER.size+NODE.size]=255
        self.validate(self.recalculate(corrupt),False)

    def test_reader_validates_resource_spans_and_kind(self):
        compiler=DensityCompiler({}, {'example:field':{'firstOctave':-3,'amplitudes':[1,0,0.25]}})
        root=compiler.compile({'type':'minecraft:noise','noise':'example:field','xz_scale':0.5,'y_scale':1})
        pack=compiler.pack(root,True); self.validate(pack,True)
        resource_offset=struct.unpack_from('<I',pack,28)[0]
        corrupt=bytearray(pack);struct.pack_into('<I',corrupt,resource_offset+16,len(pack)-1)
        self.validate(self.recalculate(corrupt),False)
        corrupt=bytearray(pack);corrupt[HEADER.size]=22
        self.validate(self.recalculate(corrupt),False)

    def test_invalid_numeric_parameters_are_rejected(self):
        for v in (math.nan,math.inf,True,1000001):
            with self.assertRaises(ValueError): DensityCompiler({},{}).compile(v)
        with self.assertRaisesRegex(ValueError,'signed 32'):
            DensityCompiler({},{}).compile({'type':'minecraft:y_clamped_gradient','from_y':-2**32,'to_y':1,'from_value':0,'to_value':1})
        with self.assertRaisesRegex(ValueError,'codec limits'):
            DensityCompiler({},{}).compile({'type':'minecraft:old_blended_noise','xz_scale':0,'y_scale':1,'xz_factor':1,'y_factor':1,'smear_scale_multiplier':1})


if __name__=='__main__': unittest.main()

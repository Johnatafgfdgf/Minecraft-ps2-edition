#!/usr/bin/env python3
"""Reject native EE single-precision arithmetic in the Java float compatibility path."""
import argparse
import pathlib
import re
import shutil
import subprocess

OBJECTS=('java_float','density_spline','simplex_noise','density_graph','density_pack','noise_chunk','noise_chunk_graph')
HELPERS=('__adddf3','__muldf3','__divdf3','__extendsfdf2')
INSTRUCTION=re.compile(r'^\s*[0-9a-f]+:\s+[0-9a-f]+\s+([a-z0-9.]+)',re.MULTILINE)


def audit(disassembly,label):
    operations=INSTRUCTION.findall(disassembly)
    if not operations: raise ValueError(f'No instructions decoded for {label}.')
    forbidden=sorted({op for op in operations if '.s' in op and op!='mov.s'})
    if forbidden: raise ValueError(f'EE float arithmetic/comparison in {label}: '+', '.join(forbidden))
    return len(operations)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--objdump',default=shutil.which('mips64r5900el-ps2-elf-objdump'))
    parser.add_argument('--build',type=pathlib.Path,default=pathlib.Path('build/ps2'))
    args=parser.parse_args()
    try:
        if not args.objdump: raise ValueError('Add the pinned EE toolchain to PATH or pass --objdump.')
        checked=0
        for name in OBJECTS:
            obj=args.build/'src/core'/f'{name}.o'
            output=subprocess.run([args.objdump,'-d',str(obj)],capture_output=True,text=True,check=True).stdout
            checked+=audit(output,str(obj))
        for helper in HELPERS:
            output=subprocess.run([args.objdump,'-d',f'--disassemble={helper}',str(args.build/'MinecraftPS2.elf')],capture_output=True,text=True,check=True).stdout
            checked+=audit(output,helper)
        print(f'EE numeric audit passed: {len(OBJECTS)} objects, {len(HELPERS)} linked binary64 helpers, {checked} instructions; no single-precision arithmetic/comparison.')
    except (OSError,ValueError,subprocess.CalledProcessError) as error:
        parser.exit(1,f'EE numeric audit failed: {error}\n')


if __name__=='__main__': main()

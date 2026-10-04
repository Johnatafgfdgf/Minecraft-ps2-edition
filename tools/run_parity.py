#!/usr/bin/env python3
"""Execute the original 1.21.1 methods and compare every emitted result with native C++."""
import argparse
import hashlib
import json
import os
import pathlib
import random
import shutil
import subprocess
from minecraft_reference import ROOT, REFERENCE, PRIVATE, load_reference, private_path


def oracle_symbols(classes):
    result = {}
    prefix = 'net.minecraft.'
    names = {
        'Legacy':'world.level.levelgen.LegacyRandomSource', 'Xoro':'world.level.levelgen.XoroshiroRandomSource',
        'Worldgen':'world.level.levelgen.WorldgenRandom', 'Support':'world.level.levelgen.RandomSupport',
        'RandomSource':'util.RandomSource', 'BlockPos':'core.BlockPos', 'SectionPos':'core.SectionPos',
        'ChunkPos':'world.level.ChunkPos', 'Storage':'util.SimpleBitStorage',
        'Tick':'world.ticks.ScheduledTick', 'TickQueue':'world.ticks.LevelChunkTicks', 'Priority':'world.ticks.TickPriority',
        'Shared':'SharedConstants', 'Bootstrap':'server.Bootstrap', 'Block':'world.level.block.Block',
        'IdMap':'core.IdMapper', 'StateHolder':'world.level.block.state.StateHolder',
        'BlockState':'world.level.block.state.BlockState', 'Property':'world.level.block.state.properties.Property',
    }
    for alias, name in names.items():
        result[alias] = classes[prefix + name]['symbol']
    methods = {
        'RandomSource': {'Random.int':'int nextInt()', 'Random.bounded':'int nextInt(int)', 'Random.long':'long nextLong()',
            'Random.boolean':'boolean nextBoolean()', 'Random.float':'float nextFloat()', 'Random.double':'double nextDouble()',
            'Random.seed':'void setSeed(long)', 'Random.fork':'net.minecraft.util.RandomSource fork()'},
        'Worldgen': {'Worldgen.decoration':'long setDecorationSeed(long,int,int)', 'Worldgen.feature':'void setFeatureSeed(long,int,int)',
            'Worldgen.large':'void setLargeFeatureSeed(long,int,int)', 'Worldgen.salt':'void setLargeFeatureWithSalt(long,int,int,int)',
            'Worldgen.slime':'net.minecraft.util.RandomSource seedSlimeChunk(int,int,long,long)', 'Worldgen.count':'int getCount()'},
        'Support': {'Support.mix':'long mixStafford13(long)'},
        'BlockPos': {'Block.pack':'long asLong(int,int,int)', 'Block.instancePack':'long asLong()', 'Block.x':'int getX(long)', 'Block.y':'int getY(long)', 'Block.z':'int getZ(long)'},
        'SectionPos': {'Section.pack':'long asLong(int,int,int)', 'Section.x':'int x(long)', 'Section.y':'int y(long)',
            'Section.z':'int z(long)', 'Section.relative':'short sectionRelativePos(net.minecraft.core.BlockPos)', 'Section.floor':'int blockToSectionCoord(int)'},
        'ChunkPos': {'Chunk.pack':'long asLong(int,int)'},
        'Storage': {'Storage.set':'void set(int,int)', 'Storage.swap':'int getAndSet(int,int)', 'Storage.get':'int get(int)', 'Storage.raw':'long[] getRaw()'},
        'TickQueue': {'TickQueue.contains':'boolean hasScheduledTick(net.minecraft.core.BlockPos,java.lang.Object)',
            'TickQueue.schedule':'void schedule(net.minecraft.world.ticks.ScheduledTick)', 'TickQueue.poll':'net.minecraft.world.ticks.ScheduledTick poll()'},
        'Tick': {'Tick.pos':'net.minecraft.core.BlockPos pos()', 'Tick.priority':'net.minecraft.world.ticks.TickPriority priority()',
            'Tick.time':'long triggerTick()', 'Tick.order':'long subTickOrder()', 'Tick.type':'java.lang.Object type()'},
        'Priority': {'Priority.byValue':'net.minecraft.world.ticks.TickPriority byValue(int)', 'Priority.value':'int getValue()'},
        'Shared': {'Shared.detect':'void tryDetectVersion()'},
        'Bootstrap': {'Bootstrap.boot':'void bootStrap()'},
        'Block': {'Block.stateMap':'net.minecraft.core.IdMapper BLOCK_STATE_REGISTRY', 'Block.stateId':'int getId(net.minecraft.world.level.block.state.BlockState)'},
        'IdMap': {'IdMap.byId':'java.lang.Object byId(int)'},
        'StateHolder': {'State.properties':'java.util.Collection getProperties()', 'State.set':'java.lang.Object setValue(net.minecraft.world.level.block.state.properties.Property,java.lang.Comparable)'},
        'Property': {'Property.name':'java.lang.String getName()', 'Property.parse':'java.util.Optional getValue(java.lang.String)'},
    }
    for alias, members in methods.items():
        for key, signature in members.items():
            result[key] = classes[prefix + names[alias]]['members'][signature]
    return result


def cases():
    generator = random.Random(0x1211_2026)
    seeds = [0,1,2,0xffffffffffffffff,0x8000000000000000,0x7fffffffffffffff,0x123456789abcdef0,0x5deece66d]
    seeds += [generator.getrandbits(64) for _ in range(8)]
    result = ['zero 128']
    for seed in seeds:
        for variant in ('legacy','xoroshiro','worldgen-legacy','worldgen-xoroshiro'):
            result.append(f'rng {variant} {seed:016x} 384')
        for variant in ('legacy','xoroshiro'):
            result.append(f'fork {variant} {seed:016x} 64')
        result.append(f'mix {seed:016x}')
    edge = [-2147483648,-33554432,-2097152,-2048,-65,-17,-16,-1,0,1,15,16,2047,2097151,33554431,2147483647]
    for x in edge:
        for y in (-2048,-64,-1,0,319,2047):
            result.append(f'pos {x} {y} {-x-1}')
    for _ in range(512):
        xyz = [generator.randint(-2147483648,2147483647) for _ in range(3)]
        result.append('pos ' + ' '.join(map(str,xyz)))
    for bits in range(1,33):
        for count in (0,1,21,65,4096):
            result.append(f'storage {bits} {count} {generator.getrandbits(32):08x}')
    for _ in range(128):
        seed=generator.getrandbits(64)
        values=[generator.randint(-2147483648,2147483647) for _ in range(5)]
        for variant in ('legacy','xoroshiro'):
            result.append(f'worldgen {variant} {seed:016x} ' + ' '.join(map(str,values)))
        result.append(f'slime {seed:016x} {values[0]} {values[1]} {generator.getrandbits(64):016x}')
    for count in (0,1,2,97,291,1024):
        for _ in range(8):
            result.append(f'ticks {generator.getrandbits(32):08x} {count}')
    return result


def java_tool(name):
    home = os.environ.get('JAVA_HOME')
    path = str(pathlib.Path(home)/'bin'/name) if home else shutil.which(name)
    if not path or not pathlib.Path(path).is_file():
        raise RuntimeError('Install JDK 21 and set JAVA_HOME (both java and javac are required).')
    return path


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference',type=pathlib.Path,default=REFERENCE)
    parser.add_argument('--runner',type=pathlib.Path,default=ROOT/'build/host/parity')
    parser.add_argument('--runner-arg',action='append',default=[])
    parser.add_argument('--cases',type=pathlib.Path)
    parser.add_argument('--output',type=pathlib.Path,default=PRIVATE/'parity')
    args=parser.parse_args()
    try:
        path,manifest,classes=load_reference(args.reference)
        output=private_path(args.output); output.mkdir(parents=True,exist_ok=True)
        configuration=oracle_symbols(classes)
        properties=output/'symbols.properties'
        properties.write_text('\n'.join(f'{key}={value}' for key,value in sorted(configuration.items()))+'\n')
        inputs=args.cases.read_text().splitlines() if args.cases else cases()
        cases_file=output/'cases.txt'; cases_file.write_text('\n'.join(inputs)+'\n')
        javac=java_tool('javac'); java=java_tool('java')
        subprocess.run([javac,'-d',str(output),str(ROOT/'tools/oracle/Oracle.java')],check=True)
        cp=os.pathsep.join([str(output)]+[str(path/name) for name in manifest['classpath']])
        original=subprocess.run([java,'-cp',cp,'mcps2.oracle.Oracle',str(properties),str(cases_file)],capture_output=True,text=True,check=True)
        native=subprocess.run([str(args.runner.resolve())]+args.runner_arg,input=cases_file.read_text(),capture_output=True,text=True,check=True)
        expected=[line for line in original.stdout.splitlines() if line.startswith('R\t')]
        actual=native.stdout.splitlines()
        (output/'original.txt').write_text('\n'.join(expected)+'\n')
        (output/'native.txt').write_text('\n'.join(actual)+'\n')
        if not expected:
            raise RuntimeError('The original-JAR oracle returned no observations.')
        if expected != actual:
            position=next((i for i,(a,b) in enumerate(zip(expected,actual)) if a!=b),min(len(expected),len(actual)))
            oracle_line=expected[position] if position<len(expected) else '<EOF>'
            native_line=actual[position] if position<len(actual) else '<EOF>'
            case=int(oracle_line.split('\t')[1]) if oracle_line!='<EOF>' else None
            raise RuntimeError(f'Parity mismatch at record {position}, input {inputs[case] if case is not None else "<EOF>"}\noriginal: {oracle_line}\nnative: {native_line}')
        report={
            'version':manifest['version'],'server_sha1':manifest['server_sha1'],
            'cases':len(inputs),'equal_records':len(expected),
            'result_sha256':hashlib.sha256(('\n'.join(expected)+'\n').encode()).hexdigest(),
            'scope':['random primitives (including forks/reseeding)','worldgen seed derivation','packed coordinates',
                     'SimpleBitStorage raw layout','per-chunk scheduled tick identity and pending order'],
            'platform':'host C++ compared to original JVM; PS2 runtime parity not yet measured',
        }
        if args.cases:
            report['scope']=['block-state property transitions via original StateHolder.setValue']
        (output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps(report,indent=2))
    except (OSError,ValueError,RuntimeError,KeyError,subprocess.CalledProcessError) as error:
        if isinstance(error,subprocess.CalledProcessError):
            parser.exit(1,f'Parity subprocess failed (exit {error.returncode}):\n{error.stderr or ""}\n')
        parser.exit(1,f'Parity failed: {error}\n')


if __name__=='__main__':
    main()

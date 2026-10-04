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
import struct
import zipfile
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
        'Noise':'world.level.levelgen.synth.ImprovedNoise',
        'Factory':'world.level.levelgen.PositionalRandomFactory', 'Seed128':'world.level.levelgen.RandomSupport$Seed128bit',
        'Mth':'util.Mth',
        'Perlin':'world.level.levelgen.synth.PerlinNoise','Normal':'world.level.levelgen.synth.NormalNoise',
        'NoiseParameters':'world.level.levelgen.synth.NormalNoise$NoiseParameters',
        'Blended':'world.level.levelgen.synth.BlendedNoise',
        'Context':'world.level.levelgen.DensityFunction$SinglePointContext','FunctionContext':'world.level.levelgen.DensityFunction$FunctionContext',
    }
    for alias, name in names.items():
        result[alias] = classes[prefix + name]['symbol']
    methods = {
        'RandomSource': {'Random.int':'int nextInt()', 'Random.bounded':'int nextInt(int)', 'Random.long':'long nextLong()',
            'Random.boolean':'boolean nextBoolean()', 'Random.float':'float nextFloat()', 'Random.double':'double nextDouble()',
            'Random.seed':'void setSeed(long)', 'Random.fork':'net.minecraft.util.RandomSource fork()',
            'Random.positional':'net.minecraft.world.level.levelgen.PositionalRandomFactory forkPositional()'},
        'Worldgen': {'Worldgen.decoration':'long setDecorationSeed(long,int,int)', 'Worldgen.feature':'void setFeatureSeed(long,int,int)',
            'Worldgen.large':'void setLargeFeatureSeed(long,int,int)', 'Worldgen.salt':'void setLargeFeatureWithSalt(long,int,int,int)',
            'Worldgen.slime':'net.minecraft.util.RandomSource seedSlimeChunk(int,int,long,long)', 'Worldgen.count':'int getCount()'},
        'Support': {'Support.mix':'long mixStafford13(long)', 'Support.hash':'net.minecraft.world.level.levelgen.RandomSupport$Seed128bit seedFromHashOf(java.lang.String)'},
        'Factory': {'Factory.at':'net.minecraft.util.RandomSource at(int,int,int)',
            'Factory.hash':'net.minecraft.util.RandomSource fromHashOf(java.lang.String)', 'Factory.seed':'net.minecraft.util.RandomSource fromSeed(long)'},
        'Seed128': {'Seed128.low':'long seedLo()', 'Seed128.high':'long seedHi()'},
        'Mth': {'Mth.seed':'long getSeed(int,int,int)'},
        'Perlin': {'Perlin.modern':'net.minecraft.world.level.levelgen.synth.PerlinNoise create(net.minecraft.util.RandomSource,int,it.unimi.dsi.fastutil.doubles.DoubleList)',
            'Perlin.legacy':'net.minecraft.world.level.levelgen.synth.PerlinNoise createLegacyForLegacyNetherBiome(net.minecraft.util.RandomSource,int,it.unimi.dsi.fastutil.doubles.DoubleList)',
            'Perlin.value':'double getValue(double,double,double)', 'Perlin.step':'double getValue(double,double,double,double,double,boolean)',
            'Perlin.max':'double maxValue()', 'Perlin.broken':'double maxBrokenValue(double)',
            'Perlin.octave':'net.minecraft.world.level.levelgen.synth.ImprovedNoise getOctaveNoise(int)', 'Perlin.wrap':'double wrap(double)'},
        'Normal': {'Normal.modern':'net.minecraft.world.level.levelgen.synth.NormalNoise create(net.minecraft.util.RandomSource,net.minecraft.world.level.levelgen.synth.NormalNoise$NoiseParameters)',
            'Normal.legacy':'net.minecraft.world.level.levelgen.synth.NormalNoise createLegacyNetherBiome(net.minecraft.util.RandomSource,net.minecraft.world.level.levelgen.synth.NormalNoise$NoiseParameters)',
            'Normal.value':'double getValue(double,double,double)', 'Normal.max':'double maxValue()'},
        'Blended': {'Blended.value':'double compute(net.minecraft.world.level.levelgen.DensityFunction$FunctionContext)',
            'Blended.min':'double minValue()', 'Blended.max':'double maxValue()',
            'Blended.reseed':'net.minecraft.world.level.levelgen.synth.BlendedNoise withNewRandom(net.minecraft.util.RandomSource)'},
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
        'Noise': {'Noise.xo':'double xo', 'Noise.yo':'double yo', 'Noise.zo':'double zo',
            'Noise.value':'double noise(double,double,double)', 'Noise.step':'double noise(double,double,double,double,double)'},
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


def noise_cases():
    generator=random.Random(0x1211_4e4f495345)
    seeds=[0,1,0xffffffffffffffff,0x8000000000000000,0x7fffffffffffffff,0x123456789abcdef0]
    seeds += [generator.getrandbits(64) for _ in range(10)]
    return [f'noise {variant} {seed:016x} 256' for seed in seeds for variant in
            ('legacy','xoroshiro','worldgen-legacy','worldgen-xoroshiro')]


def factory_cases():
    generator=random.Random(0x1211_53454544)
    seeds=[0,1,0xffffffffffffffff,0x8000000000000000,0x7fffffffffffffff,0x123456789abcdef0]
    seeds += [generator.getrandbits(64) for _ in range(10)]
    texts=['', 'minecraft:temperature','minecraft:continentalness','octave_-15','octave_0',
           'caf\u00e9','\u4e16\u754c','\U0001f642','\ud800','\udfff','\ud800x\udfff','a\0b']
    texts += ['a'*n for n in [55,56,57,63,64,65,119,120,127,128,129]]
    texts += [''.join(chr(generator.randrange(65536)) for _ in range(n)) for n in range(0,72,3)]
    result=[]
    for seed in seeds:
        for variant in ('legacy','xoroshiro','worldgen-legacy','worldgen-xoroshiro'):
            result.append(f'factory {variant} {seed:016x} 128')
            for text in texts:
                encoded=text.encode('utf-16-be',errors='surrogatepass').hex() or '-'
                result.append(f'factory-hash {variant} {seed:016x} {encoded}')
    return result


def octave_cases(reference,manifest):
    bits=lambda value:struct.pack('>d',float(value)).hex()
    modes=('modern','legacy','normal','normal-legacy')
    result=[]
    def add(variant,seed,mode,first,amplitudes,samples):
        suffix=' '.join(bits(a) for a in amplitudes)
        result.append(f'octaves {variant} {seed:016x} {mode} {first} {len(amplitudes)} {samples}' + (f' {suffix}' if suffix else ''))
    synthetic=[(-6,[1,1,1,1,1,1,1]),(-4,[1,0,0.25,0,1]),(-4,[0,0,0,0,0]),
               (-7,[1,0]),(0,[1]),(2,[1,0.5]),(-1,[1,0.5,1]),(-5,[1,0,0.1]),
               (-3,[0,0,1,0]),(-1,[]),(-8,[1,-0.25,0,1]),(-10,[1]*22)]
    for variant in ('legacy','xoroshiro','worldgen-legacy','worldgen-xoroshiro'):
        for seed in (0,0xffffffffffffffff,0x123456789abcdef0):
            for first,amplitudes in synthetic:
                for mode in modes: add(variant,seed,mode,first,amplitudes,96)
    # Original parameters stay in private cases, never copied into the public source tree.
    with zipfile.ZipFile(reference/manifest['inner_jar']) as jar:
        for name in sorted(jar.namelist()):
            if not name.startswith('data/minecraft/worldgen/noise/') or not name.endswith('.json'): continue
            parameters=json.loads(jar.read(name))
            for variant in ('legacy','xoroshiro'):
                for seed in (0,0x123456789abcdef0):
                    for mode in modes: add(variant,seed,mode,parameters['firstOctave'],parameters['amplitudes'],64)
    for x in (-1e30,-1e22,-33554432.5,-33554432.0,-16777216.5,-16777216.0,-0.0,
              0.0,0.25,16777215.5,16777216.0,33554431.5,33554432.0,1e22,1e30):
        result.append(f'wrap {bits(x)}')
    return result


def blended_cases(reference,manifest):
    configurations={(0.25,0.125,80.0,160.0,8.0),(1.0,1.0,1.0,1.0,1.0),
                    (0.001,0.001,1000.0,1000.0,8.0),(1000.0,1000.0,0.001,0.001,1.0),
                    (0.1,0.25,10.0,20.0,4.0),(0.75,0.25,3.0,7.0,2.0)}
    def scan(value):
        if isinstance(value,dict):
            if value.get('type')=='minecraft:old_blended_noise':
                configurations.add(tuple(float(value[key]) for key in ('xz_scale','y_scale','xz_factor','y_factor','smear_scale_multiplier')))
            for child in value.values(): scan(child)
        elif isinstance(value,list):
            for child in value: scan(child)
    with zipfile.ZipFile(reference/manifest['inner_jar']) as jar:
        for name in jar.namelist():
            if name.startswith(('data/minecraft/worldgen/density_function/','data/minecraft/worldgen/noise_settings/')) and name.endswith('.json'):
                scan(json.loads(jar.read(name)))
    result=[]
    for variant in ('legacy','xoroshiro','worldgen-legacy','worldgen-xoroshiro'):
        for seed in (0,1,0xffffffffffffffff,0x8000000000000000,0x123456789abcdef0):
            for parameters in sorted(configurations):
                tokens=' '.join(struct.pack('>d',p).hex() for p in parameters)
                result.append(f'blended {variant} {seed:016x} 256 {tokens}')
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference',type=pathlib.Path,default=REFERENCE)
    parser.add_argument('--runner',type=pathlib.Path,default=ROOT/'build/host/parity')
    parser.add_argument('--runner-arg',action='append',default=[])
    parser.add_argument('--cases',type=pathlib.Path)
    parser.add_argument('--output',type=pathlib.Path,default=PRIVATE/'parity')
    parser.add_argument('--suite',choices=['core','noise','factories','octaves','blended'],default='core')
    args=parser.parse_args()
    try:
        path,manifest,classes=load_reference(args.reference)
        output=private_path(args.output); output.mkdir(parents=True,exist_ok=True)
        configuration=oracle_symbols(classes)
        properties=output/'symbols.properties'
        properties.write_text('\n'.join(f'{key}={value}' for key,value in sorted(configuration.items()))+'\n')
        generators={'core':cases,'noise':noise_cases,'factories':factory_cases,'octaves':lambda:octave_cases(path,manifest),
                    'blended':lambda:blended_cases(path,manifest)}
        inputs=args.cases.read_text().splitlines() if args.cases else generators[args.suite]()
        cases_file=output/'cases.txt'; cases_file.write_text('\n'.join(inputs)+'\n')
        javac=java_tool('javac'); java=java_tool('java')
        subprocess.run([javac,'-d',str(output),str(ROOT/'tools/oracle/Oracle.java')],check=True)
        cp=os.pathsep.join([str(output)]+[str(path/name) for name in manifest['classpath']])
        original=subprocess.run([java,'-cp',cp,'mcps2.oracle.Oracle',str(properties),str(cases_file)],cwd=output,capture_output=True,text=True,check=True)
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
        elif args.suite=='noise':
            report['scope']=['ImprovedNoise construction/random consumption and noise3/noise5 binary64 results']
        elif args.suite=='factories':
            report['scope']=['positional factories, fromSeed, Java UTF-16 hash, UTF-8 MD5 seeds, fork consumption and wrapper count']
        elif args.suite=='octaves':
            report['scope']=['PerlinNoise/NormalNoise modern and Legacy construction, sparse octaves, max values, binary64 sampling and wrapping',
                             'all 60 original vanilla noise parameter definitions, kept in private fixtures']
        elif args.suite=='blended':
            report['scope']=['BlendedNoise construction/parent consumption, min/max bounds, reseeding and binary64 density samples',
                             'vanilla old_blended_noise parameter sets plus scale/coordinate boundaries']
        (output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps(report,indent=2))
    except (OSError,ValueError,RuntimeError,KeyError,subprocess.CalledProcessError) as error:
        if isinstance(error,subprocess.CalledProcessError):
            parser.exit(1,f'Parity subprocess failed (exit {error.returncode}):\n{error.stderr or ""}\n')
        parser.exit(1,f'Parity failed: {error}\n')


if __name__=='__main__':
    main()

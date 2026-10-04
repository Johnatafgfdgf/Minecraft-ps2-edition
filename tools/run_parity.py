#!/usr/bin/env python3
"""Execute the original 1.21.1 methods and compare every emitted result with native C++."""
import argparse
import hashlib
import json
import math
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
        'Simplex':'world.level.levelgen.synth.SimplexNoise',
        'End':'world.level.levelgen.DensityFunctions$EndIslandDensityFunction',
        'Cubic':'util.CubicSpline','Multipoint':'util.CubicSpline$Multipoint',
        'FloatFunction':'util.ToFloatFunction','Holder':'core.Holder',
        'SplineCoordinate':'world.level.levelgen.DensityFunctions$Spline$Coordinate',
        'Context':'world.level.levelgen.DensityFunction$SinglePointContext','FunctionContext':'world.level.levelgen.DensityFunction$FunctionContext',
        'Density':'world.level.levelgen.DensityFunction','Functions':'world.level.levelgen.DensityFunctions',
        'Vanilla':'data.registries.VanillaRegistries','Provider':'core.HolderGetter$Provider','LookupProvider':'core.HolderLookup$Provider',
        'ResourceKey':'resources.ResourceKey','Location':'resources.ResourceLocation','Registries':'core.registries.Registries',
        'RandomState':'world.level.levelgen.RandomState','Router':'world.level.levelgen.NoiseRouter',
        'Chunk':'world.level.levelgen.NoiseChunk','NoiseSettings':'world.level.levelgen.NoiseSettings',
        'GeneratorSettings':'world.level.levelgen.NoiseGeneratorSettings',
        'ChunkGenerator':'world.level.levelgen.NoiseBasedChunkGenerator',
        'BeardMarker':'world.level.levelgen.DensityFunctions$BeardifierMarker',
        'Beard':'world.level.levelgen.DensityFunctions$BeardifierOrMarker',
        'Blender':'world.level.levelgen.blending.Blender','FluidPicker':'world.level.levelgen.Aquifer$FluidPicker',
        'ContextProvider':'world.level.levelgen.DensityFunction$ContextProvider','Getter':'core.HolderGetter',
        'ChunkInterpolated':'world.level.levelgen.NoiseChunk$NoiseInterpolator',
        'ChunkFlat':'world.level.levelgen.NoiseChunk$FlatCache','ChunkColumn':'world.level.levelgen.NoiseChunk$Cache2D',
        'ChunkOnce':'world.level.levelgen.NoiseChunk$CacheOnce','ChunkCell':'world.level.levelgen.NoiseChunk$CacheAllInCell',
    }
    for alias, name in names.items():
        result[alias] = classes[prefix + name]['symbol']
    methods = {
        'Vanilla': {'Vanilla.lookup':'net.minecraft.core.HolderLookup$Provider createLookup()'},
        'LookupProvider': {'Lookup.getter':'net.minecraft.core.HolderGetter$Provider asGetterLookup()'},
        'ResourceKey': {'Key.create':'net.minecraft.resources.ResourceKey create(net.minecraft.resources.ResourceKey,net.minecraft.resources.ResourceLocation)'},
        'Location': {'Location.parse':'net.minecraft.resources.ResourceLocation parse(java.lang.String)'},
        'Registries': {'Registry.settings':'net.minecraft.resources.ResourceKey NOISE_SETTINGS'},
        'RandomState': {'RandomState.create':'net.minecraft.world.level.levelgen.RandomState create(net.minecraft.core.HolderGetter$Provider,net.minecraft.resources.ResourceKey,long)',
                        'RandomState.router':'net.minecraft.world.level.levelgen.NoiseRouter router()'},
        'Density': {'Density.value':'double compute(net.minecraft.world.level.levelgen.DensityFunction$FunctionContext)',
            'Density.min':'double minValue()', 'Density.max':'double maxValue()',
            'Density.abs':'net.minecraft.world.level.levelgen.DensityFunction abs()',
            'Density.square':'net.minecraft.world.level.levelgen.DensityFunction square()',
            'Density.cube':'net.minecraft.world.level.levelgen.DensityFunction cube()',
            'Density.half':'net.minecraft.world.level.levelgen.DensityFunction halfNegative()',
            'Density.quarter':'net.minecraft.world.level.levelgen.DensityFunction quarterNegative()',
            'Density.squeeze':'net.minecraft.world.level.levelgen.DensityFunction squeeze()',
            'Density.clamp':'net.minecraft.world.level.levelgen.DensityFunction clamp(double,double)'},
        'Functions': {'Functions.constant':'net.minecraft.world.level.levelgen.DensityFunction constant(double)',
            'Functions.spline':'net.minecraft.world.level.levelgen.DensityFunction spline(net.minecraft.util.CubicSpline)',
            'Functions.gradient':'net.minecraft.world.level.levelgen.DensityFunction yClampedGradient(int,int,double,double)',
            'Functions.add':'net.minecraft.world.level.levelgen.DensityFunction add(net.minecraft.world.level.levelgen.DensityFunction,net.minecraft.world.level.levelgen.DensityFunction)',
            'Functions.mul':'net.minecraft.world.level.levelgen.DensityFunction mul(net.minecraft.world.level.levelgen.DensityFunction,net.minecraft.world.level.levelgen.DensityFunction)',
            'Functions.min':'net.minecraft.world.level.levelgen.DensityFunction min(net.minecraft.world.level.levelgen.DensityFunction,net.minecraft.world.level.levelgen.DensityFunction)',
            'Functions.max':'net.minecraft.world.level.levelgen.DensityFunction max(net.minecraft.world.level.levelgen.DensityFunction,net.minecraft.world.level.levelgen.DensityFunction)',
            'Functions.range':'net.minecraft.world.level.levelgen.DensityFunction rangeChoice(net.minecraft.world.level.levelgen.DensityFunction,double,double,net.minecraft.world.level.levelgen.DensityFunction,net.minecraft.world.level.levelgen.DensityFunction)'},
        'FunctionContext': {'Context.y':'int blockY()'},
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
        'Simplex': {'Simplex.xo':'double xo','Simplex.yo':'double yo','Simplex.zo':'double zo',
            'Simplex.value':'double getValue(double,double)'},
        'End': {'End.noise':'net.minecraft.world.level.levelgen.synth.SimplexNoise islandNoise',
            'End.height':'float getHeightValue(net.minecraft.world.level.levelgen.synth.SimplexNoise,int,int)'},
        'Holder': {'Holder.direct':'net.minecraft.core.Holder direct(java.lang.Object)'},
        'Cubic': {'Cubic.constant':'net.minecraft.util.CubicSpline constant(float)'},
        'Multipoint': {'Multipoint.create':'net.minecraft.util.CubicSpline$Multipoint create(net.minecraft.util.ToFloatFunction,float[],java.util.List,float[])'},
    }
    for alias, members in methods.items():
        for key, signature in members.items():
            result[key] = classes[prefix + names[alias]]['members'][signature]
    extra = {
        'Density': {'Density.fill':'void fillArray(double[],net.minecraft.world.level.levelgen.DensityFunction$ContextProvider)'},
        'FunctionContext': {'Context.x':'int blockX()', 'Context.z':'int blockZ()'},
        'ContextProvider': {'Provider.index':'net.minecraft.world.level.levelgen.DensityFunction$FunctionContext forIndex(int)',
                            'Provider.direct':'void fillAllDirectly(double[],net.minecraft.world.level.levelgen.DensityFunction)'},
        'Provider': {'Getter.lookup':'net.minecraft.core.HolderGetter lookupOrThrow(net.minecraft.resources.ResourceKey)'},
        'Getter': {'Getter.get':'net.minecraft.core.Holder$Reference getOrThrow(net.minecraft.resources.ResourceKey)'},
        'Holder': {'Holder.value':'java.lang.Object value()'},
        'Blender': {'Blender.empty':'net.minecraft.world.level.levelgen.blending.Blender empty()'},
        'BeardMarker': {'Beard.instance':'net.minecraft.world.level.levelgen.DensityFunctions$BeardifierMarker INSTANCE'},
        'ChunkGenerator': {'Chunk.fluid':'net.minecraft.world.level.levelgen.Aquifer$FluidPicker createFluidPicker(net.minecraft.world.level.levelgen.NoiseGeneratorSettings)'},
        'Chunk': {'Chunk.initialize':'void initializeForFirstCellX()', 'Chunk.advance':'void advanceCellX(int)',
                  'Chunk.select':'void selectCellYZ(int,int)', 'Chunk.y':'void updateForY(int,double)',
                  'Chunk.x':'void updateForX(int,double)', 'Chunk.z':'void updateForZ(int,double)',
                  'Chunk.stop':'void stopInterpolation()', 'Chunk.swap':'void swapSlices()'},
    }
    for alias, members in extra.items():
        for key, signature in members.items(): result[key]=classes[prefix+names[alias]]['members'][signature]
    for name in ('interpolators','cellCaches','sliceFillingContextProvider','cellStartBlockX','cellStartBlockY','cellStartBlockZ',
                 'inCellX','inCellY','inCellZ','arrayIndex','interpolationCounter','arrayInterpolationCounter','interpolating','fillingCell'):
        signature=next(k for k in classes[prefix+names['Chunk']]['members'] if k.endswith(' '+name))
        result['Chunk.field.'+name]=classes[prefix+names['Chunk']]['members'][signature]
    for i,name in enumerate(('barrierNoise','fluidLevelFloodednessNoise','fluidLevelSpreadNoise','lavaNoise','temperature',
                             'vegetation','continents','erosion','depth','ridges','initialDensityWithoutJaggedness',
                             'finalDensity','veinToggle','veinRidged','veinGap')):
        result[f'Router.{i}']=classes[prefix+names['Router']]['members'][f'net.minecraft.world.level.levelgen.DensityFunction {name}()']
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


def simplex_cases():
    result=[line.replace('noise ','simplex ',1).rsplit(' ',1)[0]+' 512' for line in noise_cases()]
    for seed in (0,1,0xffffffffffffffff,0x8000000000000000,0x7fffffffffffffff,0x123456789abcdef0):
        result.append(f'end {seed:016x} 768')
    return result


def float_cases():
    generator=random.Random(0x1211_49454545)
    seeds=[0x1211,1,0xffffffff,0x80000000]+[generator.getrandbits(32) for _ in range(12)]
    return [f'java-float {seed:08x} 4096' for seed in seeds]


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


def density_cases():
    def node(op,a=0,b=0,c=0,fy=0,ty=0,p0=0,p1=0): return (op,a,b,c,fy,ty,p0,p1)
    def encode(nodes,samples=128):
        fields=[]
        for n in nodes:
            fields.extend(map(str,n[:6])); fields.extend(struct.pack('>d',float(x)).hex() for x in n[6:])
        return f'density {len(nodes)} {len(nodes)-1} {samples} ' + ' '.join(fields)
    result=[]
    values=[-1000000,-3.25,-1,-0.5,-0.0,0.0,0.125,0.5,1,2.75,1000000]
    for value in values:
        for op in range(8,14):
            for leaf in (0,14): result.append(encode([node(leaf,p0=value,p1=value),node(op)]))
    for a in values:
        for b in values:
            for op in range(2,6):
                for leaves in ((0,14),(14,0),(14,14)):
                    result.append(encode([node(leaves[0],p0=a,p1=a),node(leaves[1],p0=b,p1=b),node(op,a=0,b=1)],32))
    for fy,ty,low,high in [(-64,320,1,-1),(0,1,-0.0,0.0),(1,1,2,3),(64,-64,-2,4),
                           (-2147483648,2147483647,-3.25,0.75),(0,256,-0.0,-0.0)]:
        result.append(encode([node(1,fy=fy,ty=ty,p0=low,p1=high)]))
    for low,high in [(-1,1),(-0.0,0.0),(0.0,-0.0),(2,-2),(-100,100)]:
        result.append(encode([node(14,p0=-10,p1=10),node(6,p0=low,p1=high)]))
        result.append(encode([node(14,p0=low,p1=high),node(14,p0=-3,p1=-3),node(14,p0=7,p1=7),node(7,a=0,b=1,c=2,p0=low,p1=high)]))
    generator=random.Random(0x1211_44454e53495459)
    for _ in range(256):
        nodes=[node(1,fy=-64,ty=320,p0=-1,p1=1),node(14,fy=24,p0=-0.5,p1=0.5),node(0,p0=0.25)]
        for _ in range(generator.randrange(8,48)):
            op=generator.randrange(14); n=len(nodes)
            p0,p1=generator.choice(values),generator.choice(values)
            if op==7: p0,p1=min(p0,p1),max(p0,p1)
            nodes.append(node(op,generator.randrange(n),generator.randrange(n),generator.randrange(n),
                              generator.randrange(-64,320),generator.randrange(321,512),p0,p1))
        result.append(encode(nodes))
    result.append(encode([node(1,fy=-64,ty=320,p0=-1,p1=1),node(9),node(0,p0=0.64),node(3,a=2,b=1),node(13,a=3)]))
    return result


def spline_cases():
    d64=lambda v:struct.pack('>d',float(v)).hex()
    f32=lambda v:struct.unpack('>f',struct.pack('>f',v))[0]
    bits32=lambda v:struct.pack('>f',v).hex()
    result=[]
    def node(op,a=0,fy=0,ty=0,p0=0,p1=0): return [op,a,0,0,fy,ty,p0,p1]
    def encode(nodes,splines):
        tokens=['density',str(len(nodes)),str(len(nodes)-1),'64']
        for n in nodes: tokens += [str(v) for v in n[:6]]+[d64(v) for v in n[6:]]
        tokens.append(str(len(splines)))
        for points in splines:
            tokens.append(str(len(points)))
            for location,derivative,value in points: tokens += [bits32(location),bits32(derivative),str(value)]
        result.append(' '.join(tokens))
    coordinates=[-math.inf,-2,-1,-0.5,-0.0,0,0.5,1,2,math.inf,math.nan,
                 1+2**-24,1+3*2**-24,-1-2**-24]
    for locations in ([0],[-1,1],[-1,-0.0,0.5,1],[-1,0,0,1]):
        for derivatives in (0,-2,2,1e30):
            for coordinate in coordinates:
                nodes=[node(14,fy=24,p0=coordinate,p1=coordinate)]
                points=[]
                for i,location in enumerate(locations):
                    points.append((location,f32(derivatives*(1 if i%2==0 else -1)),len(nodes)))
                    nodes.append(node(0,p0=f32([-0.0,0.25,-1.25,2.5][i%4])))
                nodes.append(node(35,a=0,fy=0));encode(nodes,[points])
    generator=random.Random(0x1211_53504c494e45)
    for _ in range(192):
        nodes=[node(14,fy=24,p0=-2,p1=2),node(1,fy=-64,ty=320,p0=-3,p1=3)]
        splines=[];values=[]
        for _ in range(generator.randrange(2,8)):
            points=[]
            for location in (-2,-0.5,0,0.5,2):
                if values and generator.randrange(3)==0: child=generator.choice(values)
                else:
                    child=len(nodes);nodes.append(node(0,p0=f32(generator.uniform(-4,4))))
                points.append((location,f32(generator.choice([0,-0.0,generator.uniform(-3,3)])),child))
            coordinate=generator.randrange(2);values.append(len(nodes))
            nodes.append(node(35,a=coordinate,fy=len(splines)));splines.append(points)
        encode(nodes,splines)
    return result


def density_data_cases(reference,manifest):
    from compile_density_graph import compile_inventory,ROUTER_FIELDS
    report=compile_inventory(reference)
    result=[]
    for e in report['entries']:
        if e['status']!='point_graph': continue
        for seed in (0,1,0xffffffffffffffff,0x8000000000000000,0x7fffffffffffffff,0x123456789abcdef0):
            result.append(f"density-data {e['setting']} {ROUTER_FIELDS.index(e['field'])} {seed:016x} 256 {e['path'].encode().hex()}")
    return result


def noise_chunk_cases():
    result=[]
    configurations=[(4,8,-64,24,2,-17,-33,2),(8,4,-16,16,2,7,-9,1),
                    (16,16,0,32,1,-16,16,1),(4,4,-16,16,4,0,0,1),
                    (4,8,-64,384,1,-30000000,29999999,1),(8,4,0,128,1,2147483632,-2147483648,1)]
    for i,(width,height,min_y,total_height,count,x,z,cycles) in enumerate(configurations):
        for mode in range(3):
            for style in range(2):
                result.append(f'chunk {width} {height} {min_y} {total_height} {count} {x} {z} {mode} {style} {cycles} {((i+mode)*0x9e3779b97f4a7c15)&((1<<64)-1):016x}')
    # Bind native graphs to real wrapper inputs, matching original operator
    # factories around those wrappers. Keep the old 36 fixtures unchanged.
    for i in (0,1,4):
        width,height,min_y,total_height,count,x,z,cycles=configurations[i]
        for mode in range(3,8):
            for style in range(2):
                result.append(f'chunk {width} {height} {min_y} {total_height} {count} {x} {z} {mode} {style} {cycles} {((i+mode)*0x9e3779b97f4a7c15)&((1<<64)-1):016x}')
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference',type=pathlib.Path,default=REFERENCE)
    parser.add_argument('--runner',type=pathlib.Path,default=ROOT/'build/host/parity')
    parser.add_argument('--runner-arg',action='append',default=[])
    parser.add_argument('--cases',type=pathlib.Path)
    parser.add_argument('--output',type=pathlib.Path,default=PRIVATE/'parity')
    parser.add_argument('--suite',choices=['core','noise','simplex','java-float','factories','octaves','blended','density','density-spline','density-data','noise-chunk','density-batch','density-data-batch'],default='core')
    args=parser.parse_args()
    try:
        path,manifest,classes=load_reference(args.reference)
        output=private_path(args.output); output.mkdir(parents=True,exist_ok=True)
        configuration=oracle_symbols(classes)
        properties=output/'symbols.properties'
        properties.write_text('\n'.join(f'{key}={value}' for key,value in sorted(configuration.items()))+'\n')
        generators={'core':cases,'noise':noise_cases,'simplex':simplex_cases,'java-float':float_cases,'factories':factory_cases,'octaves':lambda:octave_cases(path,manifest),
                    'blended':lambda:blended_cases(path,manifest),'density':density_cases,'density-spline':spline_cases,
                    'density-data':lambda:density_data_cases(path,manifest),'noise-chunk':noise_chunk_cases,
                    'density-batch':lambda:[s.replace('density ','density-batch ',1) for s in density_cases()+spline_cases()],
                    'density-data-batch':lambda:[s.replace('density-data ','density-data-batch ',1) for s in density_data_cases(path,manifest)]}
        inputs=args.cases.read_text().splitlines() if args.cases else generators[args.suite]()
        cases_file=output/'cases.txt'; cases_file.write_text('\n'.join(inputs)+'\n')
        javac=java_tool('javac'); java=java_tool('java')
        subprocess.run([javac,'-d',str(output),str(ROOT/'tools/oracle/Oracle.java'),str(ROOT/'tools/oracle/NoiseChunkOracle.java'),str(ROOT/'tools/oracle/DensityBatchOracle.java')],check=True)
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
        elif args.suite=='simplex':
            report['scope']=['SimplexNoise construction, offsets, parent consumption and 2D binary64 samples including ties',
                             'EndIslandDensityFunction heights/density at central, outer and overflow coordinates; six world seeds',
                             'Simplex 3D and complete End chunks are not yet covered']
        elif args.suite=='java-float':
            report['reference_kind']='authored Java 21 expressions; this suite does not invoke Minecraft methods'
            report['scope']=['authored Java 21 strict binary32 arithmetic against the portable native compatibility layer',
                             'add/sub/mul/div/remainder, min/max/comparison, sqrt via double, double/int conversions; bit patterns include subnormal, overflow, signed zero, NaN and infinity',
                             'EE code generation is audited separately; runtime output and performance still need hardware measurement']
        elif args.suite=='factories':
            report['scope']=['positional factories, fromSeed, Java UTF-16 hash, UTF-8 MD5 seeds, fork consumption and wrapper count']
        elif args.suite=='octaves':
            report['scope']=['PerlinNoise/NormalNoise modern and Legacy construction, sparse octaves, max values, binary64 sampling and wrapping',
                             'all 60 original vanilla noise parameter definitions, kept in private fixtures']
        elif args.suite=='blended':
            report['scope']=['BlendedNoise construction/parent consumption, min/max bounds, reseeding and binary64 density samples',
                             'vanilla old_blended_noise parameter sets plus scale/coordinate boundaries']
        elif args.suite=='density':
            report['scope']=['DensityFunctions arithmetic/gradient/map/clamp/range-choice construction and declared bounds',
                             'binary64 finite values, signed zeros, NaN semantics and lazy leaf evaluation order',
                             'synthetic graphs; chunk interpolation and complete vanilla router evaluation not yet covered']
        elif args.suite=='density-spline':
            report['scope']=['original CubicSpline factories and DensityFunctions.Spline compute/bounds',
                             'binary32 Hermite/extrapolation, nested values, zero derivatives, signed zero, NaN and lazy coordinate order',
                             'synthetic graphs; vanilla routers and NoiseChunk integration are covered in separate suites']
        elif args.suite=='density-data':
            report['scope']=['verified vanilla JSON -> private MCDG -> native point graphs compared independently with original RandomState routers',
                             'seeded noise binding including Legacy climate/offset exceptions, named references and point markers',
                             'all 105 vanilla router fields, including splines, weird_scaled_sampler and end_islands',
                             'NoiseChunk interpolation, aquifers and full chunk generation not yet covered']
            inventory=json.loads((PRIVATE/'density-data/inventory.json').read_text())
            report['graph_counts']=inventory['counts']
        elif args.suite=='noise-chunk':
            report['scope']=['original NoiseChunk constructors, five runtime cache wrappers and complete cell/slice lifecycle',
                             'binary64 interpolation (Y/X/Z traversal versus X/Y/Z filling), context identity, counters, bulk callbacks and leaf call trace',
                             'synthetic wrapper wiring with seeded ImprovedNoise, original gradient fields and arithmetic/range graphs around cache inputs',
                             'automatic vanilla router visitor and full chunks remain pending']
        elif args.suite=='density-batch':
            report['scope']=['original fillArray paths for arithmetic, constants, gradients, maps, range choice and nested splines',
                             'synthetic graph values and ordered provider/leaf callback trace; externally buffered iterative native evaluation']
        elif args.suite=='density-data-batch':
            report['scope']=['original RandomState router fillArray for all 105 vanilla fields at six world seeds',
                             'binary64 values and ordered direct/index provider trace; point contexts with empty Blender',
                             'automatic NoiseChunk visitor and full chunk generation remain pending']
        (output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps(report,indent=2))
    except (OSError,ValueError,RuntimeError,KeyError,subprocess.CalledProcessError) as error:
        if isinstance(error,subprocess.CalledProcessError):
            # Bootstrap routes exception stacks through its stdout logger.
            diagnostics='\n'.join(((error.stdout or '')+'\n'+(error.stderr or '')).splitlines()[-40:])
            parser.exit(1,f'Parity subprocess failed (exit {error.returncode}):\n{diagnostics}\n')
        parser.exit(1,f'Parity failed: {error}\n')


if __name__=='__main__':
    main()

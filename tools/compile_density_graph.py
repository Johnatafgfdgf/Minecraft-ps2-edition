#!/usr/bin/env python3
"""Convert verified local density definitions to an independent point-graph format."""
import argparse
import collections
import json
import math
import pathlib
import re
import struct
import zipfile
import zlib
from minecraft_reference import PRIVATE, REFERENCE, load_reference, private_path

OPS = dict(zip(('constant','y_clamped_gradient','add','mul','min','max','clamp','range_choice',
                'abs','square','cube','half_negative','quarter_negative','squeeze'),range(14)))
OPS.update(dict(zip(('noise','shift','shift_a','shift_b','shifted_noise','old_blended_noise',
                     'interpolated','flat_cache','cache_2d','cache_once','cache_all_in_cell',
                     'blend_alpha','blend_offset','blend_density','reference','beardifier'),range(17,33))))
OPS.update(weird_scaled_sampler=33,end_islands=34,spline=35,spline_constant=36)
ROUTER_FIELDS = ('barrier','fluid_level_floodedness','fluid_level_spread','lava','temperature',
                 'vegetation','continents','erosion','depth','ridges','initial_density_without_jaggedness',
                 'final_density','vein_toggle','vein_ridged','vein_gap')
NODE = struct.Struct('<B3xIIIiidd')
RESOURCE = struct.Struct('<IIIiIIddddd')
HEADER = struct.Struct('<4s13I')
SPLINE = struct.Struct('<IIII')
POINT = struct.Struct('<ffII')


class UnsupportedDensity(ValueError):
    pass


def resource_name(value):
    if not isinstance(value,str): raise ValueError('A registry reference must be a resource name.')
    value=value if ':' in value else 'minecraft:'+value
    if not re.fullmatch(r'[a-z0-9_.-]+:[a-z0-9_./-]+',value): raise ValueError('Invalid resource name.')
    return value


def number(value):
    if isinstance(value,bool) or not isinstance(value,(int,float)) or not math.isfinite(value):
        raise ValueError('Density parameters must be finite numbers.')
    return float(value)


def integer(value):
    if isinstance(value,bool) or not isinstance(value,int) or not -2147483648<=value<=2147483647:
        raise ValueError('Density integer outside signed 32-bit range.')
    return value


def single(value):
    value=number(value)
    try: return struct.unpack('<f',struct.pack('<f',value))[0]
    except OverflowError as error: raise ValueError('Spline number outside binary32 range.') from error


class DensityCompiler:
    def __init__(self,definitions,noises):
        self.definitions=definitions; self.noises=noises
        self.nodes=[]; self.resources=[]; self.references={}; self.active=[]; self.resource_ids={}
        self.splines=[]; self.points=[]

    def emit(self,op,a=0,b=0,c=0,fy=0,ty=0,p0=0,p1=0):
        self.nodes.append((OPS[op],a,b,c,fy,ty,p0,p1)); return len(self.nodes)-1

    def noise_resource(self,name):
        name=resource_name(name); key=('normal',name)
        if key not in self.resource_ids:
            if name not in self.noises: raise ValueError(f'Missing noise registry entry: {name}')
            p=self.noises[name]
            first=integer(p['firstOctave']); amplitudes=[number(v) for v in p['amplitudes']]
            self.resource_ids[key]=len(self.resources)
            self.resources.append(dict(kind=0,name=name,first=first,amplitudes=amplitudes,parameters=[0]*5))
        return self.resource_ids[key]

    def compile_spline(self,value):
        if isinstance(value,(int,float)) and not isinstance(value,bool): return self.emit('constant',p0=single(value))
        if not isinstance(value,dict) or 'coordinate' not in value or 'points' not in value or not isinstance(value['points'],list):
            raise ValueError('Invalid spline definition.')
        coordinate=self.compile(value['coordinate']); points=[]
        if not value['points']: raise ValueError('Spline requires at least one point.')
        for point in value['points']:
            if not isinstance(point,dict) or any(k not in point for k in ('location','derivative','value')):
                raise ValueError('Invalid spline point.')
            points.append((single(point['location']),single(point['derivative']),self.compile_spline(point['value']),0))
        first=len(self.points);self.points.extend(points);index=len(self.splines)
        self.splines.append((coordinate,first,len(points),0))
        return self.emit('spline',a=coordinate,fy=index)

    def compile(self,value):
        if isinstance(value,(int,float)) and not isinstance(value,bool):
            v=number(value)
            if not -1000000<=v<=1000000: raise ValueError('Constant outside original noise-value codec range.')
            return self.emit('constant',p0=v)
        if isinstance(value,str):
            name=resource_name(value)
            if name in self.active: raise ValueError('Cyclic density reference: '+' -> '.join(self.active+[name]))
            if name not in self.references:
                if name not in self.definitions: raise ValueError(f'Missing density registry entry: {name}')
                self.active.append(name)
                try: target=self.compile(self.definitions[name])
                finally: self.active.pop()
                self.references[name]=self.emit('reference',a=target)
            return self.references[name]
        if not isinstance(value,dict) or 'type' not in value: raise ValueError('Invalid density definition.')
        kind=resource_name(value['type'])
        if not kind.startswith('minecraft:') or kind[10:] not in OPS or kind in ('minecraft:reference','minecraft:spline_constant'):
            raise UnsupportedDensity(f'Unsupported {kind}; dependency: '+(' -> '.join(self.active) or '<router root>'))
        kind=kind[10:]
        if kind=='constant': return self.compile(value['argument'])
        if kind=='y_clamped_gradient':
            return self.emit(kind,fy=integer(value['from_y']),ty=integer(value['to_y']),p0=number(value['from_value']),p1=number(value['to_value']))
        if kind in ('add','mul','min','max'):
            return self.emit(kind,a=self.compile(value['argument1']),b=self.compile(value['argument2']))
        if kind=='clamp':
            return self.emit(kind,a=self.compile(value['input']),p0=number(value['min']),p1=number(value['max']))
        if kind=='range_choice':
            return self.emit(kind,a=self.compile(value['input']),b=self.compile(value['when_in_range']),c=self.compile(value['when_out_of_range']),
                             p0=number(value['min_inclusive']),p1=number(value['max_exclusive']))
        if kind in ('abs','square','cube','half_negative','quarter_negative','squeeze',
                    'interpolated','flat_cache','cache_2d','cache_once','cache_all_in_cell','blend_density'):
            return self.emit(kind,a=self.compile(value['argument']))
        if kind in ('blend_alpha','blend_offset','beardifier'): return self.emit(kind)
        if kind=='spline':
            if isinstance(value['spline'],(int,float)) and not isinstance(value['spline'],bool):
                return self.emit('spline_constant',p0=single(value['spline']))
            return self.compile_spline(value['spline'])
        if kind=='end_islands':
            key=('end',)
            if key not in self.resource_ids:
                self.resource_ids[key]=len(self.resources)
                self.resources.append(dict(kind=2,name='minecraft:end_islands',first=0,amplitudes=[],parameters=[0]*5))
            return self.emit(kind,fy=self.resource_ids[key])
        if kind=='weird_scaled_sampler':
            mapper=value['rarity_value_mapper']
            if mapper not in ('type_1','type_2'): raise ValueError('Unknown rarity-value mapper.')
            return self.emit(kind,a=self.compile(value['input']),fy=self.noise_resource(value['noise']),ty=int(mapper=='type_2'))
        if kind in ('noise','shift','shift_a','shift_b','shifted_noise'):
            index=self.noise_resource(value['noise'] if kind in ('noise','shifted_noise') else value['argument'])
            if kind=='shifted_noise':
                return self.emit(kind,a=self.compile(value['shift_x']),b=self.compile(value['shift_y']),c=self.compile(value['shift_z']),fy=index,
                                 p0=number(value['xz_scale']),p1=number(value['y_scale']))
            if kind=='noise': return self.emit(kind,fy=index,p0=number(value['xz_scale']),p1=number(value['y_scale']))
            return self.emit(kind,fy=index)
        if kind=='old_blended_noise':
            parameters=[number(value[k]) for k in ('xz_scale','y_scale','xz_factor','y_factor','smear_scale_multiplier')]
            if any(not 0.001<=v<=1000 for v in parameters[:4]) or not 1<=parameters[4]<=8:
                raise ValueError('Blended parameters outside original codec limits.')
            key=('blended',*parameters)
            if key not in self.resource_ids:
                self.resource_ids[key]=len(self.resources)
                self.resources.append(dict(kind=1,name='minecraft:terrain',first=0,amplitudes=[],parameters=parameters))
            return self.emit(kind,fy=self.resource_ids[key])
        raise UnsupportedDensity(kind)

    def pack(self,root,legacy):
        if not self.nodes or not 0<=root<len(self.nodes): raise ValueError('Invalid density root.')
        node_offset=HEADER.size; resource_offset=node_offset+len(self.nodes)*NODE.size
        spline_offset=resource_offset+len(self.resources)*RESOURCE.size; point_offset=spline_offset+len(self.splines)*SPLINE.size
        payload=bytearray(b''.join(NODE.pack(*n) for n in self.nodes))
        payload.extend(bytes(len(self.resources)*RESOURCE.size))
        payload.extend(b''.join(SPLINE.pack(*s) for s in self.splines)); payload.extend(b''.join(POINT.pack(*p) for p in self.points))
        for i,r in enumerate(self.resources):
            name=r['name'].encode('ascii'); name_offset=HEADER.size+len(payload); payload.extend(name)
            payload.extend(bytes((-len(payload)-HEADER.size)%8))
            amplitude_offset=HEADER.size+len(payload)
            payload.extend(b''.join(struct.pack('<d',v) for v in r['amplitudes']))
            start=resource_offset-HEADER.size+i*RESOURCE.size
            payload[start:start+RESOURCE.size]=RESOURCE.pack(r['kind'],name_offset,len(name),r['first'],amplitude_offset,len(r['amplitudes']),*r['parameters'])
        total=HEADER.size+len(payload)
        return HEADER.pack(b'MCDG',3,2|int(legacy),len(self.nodes),root,len(self.resources),node_offset,resource_offset,total,zlib.crc32(payload),
                           len(self.splines),len(self.points),spline_offset,point_offset)+payload


def read_worldgen(reference,manifest):
    result=[]
    with zipfile.ZipFile(reference/manifest['inner_jar']) as jar:
        for family in ('density_function','noise','noise_settings'):
            prefix=f'data/minecraft/worldgen/{family}/'
            result.append({'minecraft:'+name[len(prefix):-5]:json.loads(jar.read(name)) for name in sorted(jar.namelist()) if name.startswith(prefix) and name.endswith('.json')})
    return result


def compile_inventory(reference=REFERENCE,output=PRIVATE/'density-data'):
    reference,manifest,_=load_reference(reference); output=private_path(output); output.mkdir(parents=True,exist_ok=True)
    definitions,noises,settings=read_worldgen(reference,manifest); entries=[]
    for setting,configuration in settings.items():
        for field in ROUTER_FIELDS:
            compiler=DensityCompiler(definitions,noises)
            try: root=compiler.compile(configuration['noise_router'][field])
            except UnsupportedDensity as error:
                entries.append(dict(setting=setting,field=field,status='unsupported',reason=str(error))); continue
            target=output/(setting.replace(':','_')+'_'+field+'.mcdg')
            target.write_bytes(compiler.pack(root,configuration['legacy_random_source']))
            entries.append(dict(setting=setting,field=field,status='point_graph',path=str(target),nodes=len(compiler.nodes),resources=len(compiler.resources),
                                splines=len(compiler.splines),points=len(compiler.points),bytes=target.stat().st_size))
    report=dict(version=manifest['version'],server_sha1=manifest['server_sha1'],domain='SinglePointContext; no NoiseChunk wrappers',
                counts=dict(collections.Counter(e['status'] for e in entries)),entries=entries)
    (output/'inventory.json').write_text(json.dumps(report,indent=2)+'\n'); return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference',type=pathlib.Path,default=REFERENCE)
    parser.add_argument('--output',type=pathlib.Path,default=PRIVATE/'density-data')
    parser.add_argument('--inventory',action='store_true',help='Compile supported point graphs and explicitly inventory unavailable roots.')
    args=parser.parse_args()
    if not args.inventory: parser.error('Use --inventory; complete chunk/world generation is not implemented.')
    try: report=compile_inventory(args.reference,args.output)
    except (ValueError,KeyError,OSError) as error: parser.exit(1,f'Density conversion failed: {error}\n')
    print(json.dumps({k:v for k,v in report.items() if k!='entries'},indent=2))


if __name__=='__main__': main()

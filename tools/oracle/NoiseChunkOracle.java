// Authored test driver. All chunk/cache/lerp operations execute in the verified
// local original JAR. Reflection names come from that JAR's legal mappings.
package mcps2.oracle;
import java.lang.reflect.*;
import java.util.*;
import static mcps2.oracle.Oracle.*;

class NoiseChunkOracle {
    Object chunk;
    Object[] functions;
    long trace=0, samples=0, fills=0;
    int width,height,minY,totalHeight,countXZ,firstX,firstZ,mode,style,cycles;
    long seed;
    final Map<String,Field> fields=new HashMap<>();
    static Object settings,randomState;
    static final long[] EDGES={0,0x8000000000000000L,0x3ff0000000000000L,0xbff0000000000000L,
        0x3fb999999999999aL,0x7ff0000000000000L,0xfff0000000000000L,0x7ff8000000000000L};
    void mix(long value) { trace=(trace*0x100000001b3L)^value; }
    Field field(String name) throws Exception {
        Field result=fields.get(name);
        if (result==null) {
            result=type("Chunk").getDeclaredField(symbols.getProperty("Chunk.field."+name));
            result.setAccessible(true);fields.put(name,result);
        }
        return result;
    }
    int integer(String name) throws Exception { return field(name).getInt(chunk); }
    long counter(String name) throws Exception { return field(name).getLong(chunk); }
    boolean flag(String name) throws Exception { return field(name).getBoolean(chunk); }
    void state(String label) throws Exception {
        String record="chunk-state "+label;
        for (String name:new String[]{"cellStartBlockX","cellStartBlockY","cellStartBlockZ","inCellX","inCellY","inCellZ","arrayIndex"}) record+=" "+integer(name);
        record+=" "+h64(counter("interpolationCounter"))+" "+h64(counter("arrayInterpolationCounter"));
        record+=" "+(flag("interpolating")?1:0)+" "+(flag("fillingCell")?1:0);
        emit(record+" "+h64(trace)+" "+samples+" "+fills);
    }
    Object point(int x,int y,int z) throws Exception { return type("Context").getConstructor(XYZ).newInstance(x,y,z); }
    int coord(Object context,String key) throws Exception {
        return (int)method(key,type("FunctionContext")).invoke(context);
    }
    Object leaf(int id) throws Exception {
        Object noise=type("Noise").getConstructor(type("RandomSource")).newInstance(random("legacy",seed^(id==0?0:0x9e3779b97f4a7c15L)));
        Object gradient=staticCall("Functions.gradient","Functions",new Class<?>[]{int.class,int.class,double.class,double.class},-65,104,-1.3,2.7);
        return Proxy.newProxyInstance(type("Density").getClassLoader(),new Class<?>[]{type("Density")},(proxy,m,args)-> {
            if (m.getName().equals(symbols.getProperty("Density.value")) && args!=null && args.length==1) {
                Object context=args[0];int x=coord(context,"Context.x"),y=coord(context,"Context.y"),z=coord(context,"Context.z");
                mix(0x10+id);mix(Integer.toUnsignedLong(x));mix(Integer.toUnsignedLong(y));mix(Integer.toUnsignedLong(z));
                if (context==chunk) {
                    mix(counter("interpolationCounter"));mix(counter("arrayInterpolationCounter"));mix(Integer.toUnsignedLong(integer("arrayIndex")));mix(flag("fillingCell")?1:0);
                } else mix(-1L);
                ++samples;
                if (mode==1) return Double.longBitsToDouble(EDGES[((x>>>2)+(y>>>2)+(z>>>2)+id)&7]);
                if (mode==2 && id==0) return densityCall("Density.value",gradient,new Class<?>[]{type("FunctionContext")},context);
                return (double)method("Noise.value",type("Noise"),double.class,double.class,double.class).invoke(noise,x*0.03125,y*0.0625,z*0.015625)+id*0.125;
            }
            if (m.getName().equals(symbols.getProperty("Density.fill")) && args!=null && args.length==2) {
                double[] out=(double[])args[0];Object provider=args[1];
                mix(0x100+id);mix(out.length);mix(counter("arrayInterpolationCounter"));++fills;
                if (style==0) method("Provider.direct",type("ContextProvider"),double[].class,type("Density")).invoke(provider,out,proxy);
                else for (int i=0;i<out.length;++i) {
                    Object context=method("Provider.index",type("ContextProvider"),int.class).invoke(provider,i);
                    out[i]=(double)densityCall("Density.value",proxy,new Class<?>[]{type("FunctionContext")},context);
                }
                return null;
            }
            if (m.getName().equals(symbols.getProperty("Density.min")) && (args==null || args.length==0)) return -1e12;
            if (m.getName().equals(symbols.getProperty("Density.max")) && (args==null || args.length==0)) return 1e12;
            if (m.getName().equals("equals")) return proxy==args[0];
            if (m.getName().equals("hashCode")) return System.identityHashCode(proxy);
            if (m.getName().equals("toString")) return "authored-chunk-probe-"+id;
            throw new UnsupportedOperationException(m.toString());
        });
    }
    Object wrap(String name,Object child,boolean initialize) throws Exception {
        Class<?>[] signature=name.equals("ChunkColumn")?new Class<?>[]{type("Density")}:name.equals("ChunkFlat")?
            new Class<?>[]{type("Chunk"),type("Density"),boolean.class}:new Class<?>[]{type("Chunk"),type("Density")};
        Constructor<?> ctor=type(name).getDeclaredConstructor(signature);ctor.setAccessible(true);
        return name.equals("ChunkColumn")?ctor.newInstance(child):name.equals("ChunkFlat")?ctor.newInstance(chunk,child,initialize):ctor.newInstance(chunk,child);
    }
    static Throwable cause(Throwable e) {
        while (e instanceof InvocationTargetException || e instanceof UndeclaredThrowableException) e=e.getCause();
        return e;
    }
    static int status(Throwable e) throws Exception {
        e=cause(e);
        if (e instanceof IllegalStateException) return 1;
        if (e instanceof IndexOutOfBoundsException) return 2;
        throw new Exception("Unexpected original failure",e);
    }
    String sample(Object function,Object context) throws Exception {
        try { return "0:"+h64(Double.doubleToLongBits((double)densityCall("Density.value",function,new Class<?>[]{type("FunctionContext")},context))); }
        catch (Throwable e) { return status(e)+":"+h64(0); }
    }
    void values(String label,Object context) throws Exception {
        String record="chunk-values "+label;
        for (Object function:functions) record+=" "+sample(function,context);
        record+=" "+sample(functions[5],context)+" "+sample(functions[8],context);
        emit(record+" "+h64(trace)+" "+samples+" "+fills);
    }
    void operation(String label,String key,Class<?>[] signature,Object... args) throws Exception {
        int code=0;
        try { method(key,type("Chunk"),signature).invoke(chunk,args); } catch (Throwable e) { code=status(e); }
        emit("chunk-operation "+label+" "+code);
    }
    void fillArray(String label,Object function,Object provider,int length) throws Exception {
        double[] out=new double[length];int code=0;
        try { densityCall("Density.fill",function,new Class<?>[]{double[].class,type("ContextProvider")},out,provider); }
        catch (Throwable e) { code=status(e); }
        long hash=0;
        for (double value:out) hash=(hash*0x100000001b3L)^Double.doubleToLongBits(value);
        emit("chunk-array "+label+" "+code+" "+h64(hash)+" "+h64(trace)+" "+samples+" "+fills);
    }
    void constructChunk() throws Exception {
        ensureBootstrap();
        if (settings==null) {
            Object lookup=staticCall("Vanilla.lookup","Vanilla",NONE);
            Object provider=method("Lookup.getter",type("LookupProvider")).invoke(lookup);
            Object registry=type("Registries").getField(symbols.getProperty("Registry.settings")).get(null);
            Object location=staticCall("Location.parse","Location",new Class<?>[]{String.class},"minecraft:overworld");
            Object key=staticCall("Key.create","ResourceKey",new Class<?>[]{type("ResourceKey"),type("Location")},registry,location);
            Object getter=method("Getter.lookup",type("Provider"),type("ResourceKey")).invoke(provider,registry);
            Object holder=method("Getter.get",type("Getter"),type("ResourceKey")).invoke(getter,key);
            settings=method("Holder.value",type("Holder")).invoke(holder);
            randomState=staticCall("RandomState.create","RandomState",new Class<?>[]{type("Provider"),type("ResourceKey"),long.class},provider,key,0L);
        }
        Object noiseSettings=type("NoiseSettings").getConstructor(int.class,int.class,int.class,int.class).newInstance(minY,totalHeight,width/4,height/4);
        Object beard=type("BeardMarker").getField(symbols.getProperty("Beard.instance")).get(null);
        Method picker=type("ChunkGenerator").getDeclaredMethod(symbols.getProperty("Chunk.fluid"),type("GeneratorSettings"));picker.setAccessible(true);
        Object fluid=picker.invoke(null,settings),blender=staticCall("Blender.empty","Blender",NONE);
        chunk=type("Chunk").getConstructor(int.class,type("RandomState"),int.class,int.class,type("NoiseSettings"),type("Beard"),type("GeneratorSettings"),type("FluidPicker"),type("Blender"))
            .newInstance(countXZ,randomState,firstX,firstZ,noiseSettings,beard,settings,fluid,blender);
        // Isolate authored probe wiring after the real constructor. This suite
        // tests wrapper/lifecycle semantics, not the vanilla router visitor.
        ((List<?>)field("interpolators").get(chunk)).clear();((List<?>)field("cellCaches").get(chunk)).clear();
    }
    void construct() throws Exception {
        constructChunk();
        Object a=leaf(0),b=leaf(1);
        functions=new Object[10];
        functions[0]=wrap("ChunkOnce",a,true);functions[1]=wrap("ChunkColumn",b,true);
        functions[2]=wrap("ChunkFlat",functions[0],true);
        functions[3]=wrap("ChunkInterpolated",functions[0],true);functions[4]=wrap("ChunkInterpolated",functions[1],true);
        functions[5]=wrap("ChunkOnce",functions[3],true);
        Object cellChild=functions[5];
        if (mode>=3) {
            String[] binary={"Functions.add","Functions.mul","Functions.min","Functions.max"};
            if (mode<=6) cellChild=staticCall(binary[mode-3],"Functions",new Class<?>[]{type("Density"),type("Density")},functions[5],functions[1]);
            else {
                Object outside=staticCall("Functions.constant","Functions",new Class<?>[]{double.class},0.375);
                cellChild=staticCall("Functions.range","Functions",new Class<?>[]{type("Density"),double.class,double.class,type("Density"),type("Density")},functions[5],-0.15,0.2,functions[1],outside);
            }
        }
        functions[6]=wrap("ChunkCell",cellChild,true);functions[7]=wrap("ChunkCell",cellChild,true);
        functions[8]=wrap("ChunkOnce",cellChild,true);functions[9]=wrap("ChunkFlat",functions[1],false);
    }
    void execute() throws Exception {
        construct();state("construct");
        for (int i=0;i<functions.length;++i) emit("chunk-bounds "+i+" "+h64(Double.doubleToLongBits((double)densityCall("Density.min",functions[i],NONE)))
            +" "+h64(Double.doubleToLongBits((double)densityCall("Density.max",functions[i],NONE))));
        emit("chunk-sentinel "+sample(functions[1],point(1875066,9,1875066))+" "+h64(trace)+" "+samples+" "+fills);
        values("inactive",chunk);
        int[][] edges={{firstX,minY,firstZ},{firstX,minY+height,firstZ},{firstX-1,minY,firstZ-1},
            {firstX+countXZ*width+4,minY,firstZ+countXZ*width+4},{Integer.MIN_VALUE,-1,Integer.MAX_VALUE}};
        for (int i=0;i<edges.length;++i) values("external-"+i,point(edges[i][0],edges[i][1],edges[i][2]));
        int length=width*width*height;
        fillArray("once-first",functions[0],chunk,length);
        emit("chunk-array-owner "+sample(functions[0],chunk)+" "+h64(trace)+" "+samples+" "+fills);
        fillArray("once-short",functions[0],chunk,3);fillArray("once-long",functions[0],chunk,length+1);
        for (int index:new int[]{-width*width,-1,0,length-1,length}) {
            method("Provider.index",type("ContextProvider"),int.class).invoke(chunk,index);state("index-"+index);
        }
        for (int cycle=0;cycle<cycles;++cycle) {
            operation("initialize","Chunk.initialize",NONE);state("initialize-"+cycle);
            operation("initialize-twice","Chunk.initialize",NONE);
            for (int cx=0;cx<countXZ;++cx) {
                operation("advance","Chunk.advance",INT,cx);state("advance-"+cycle+"-"+cx);
                for (int cz=0;cz<countXZ;++cz) for (int cy=totalHeight/height-1;cy>=0;--cy) {
                    operation("select","Chunk.select",new Class<?>[]{int.class,int.class},cy,cz);
                    state("select-"+cycle+"-"+cx+"-"+cy+"-"+cz);
                    int sx=integer("cellStartBlockX"),sy=integer("cellStartBlockY"),sz=integer("cellStartBlockZ");
                    for (int y=height-1;y>=0;--y) {
                        method("Chunk.y",type("Chunk"),int.class,double.class).invoke(chunk,sy+y,y/(double)height);
                        for (int x=0;x<width;++x) {
                            method("Chunk.x",type("Chunk"),int.class,double.class).invoke(chunk,sx+x,x/(double)width);
                            for (int z=0;z<width;++z) {
                                method("Chunk.z",type("Chunk"),int.class,double.class).invoke(chunk,sz+z,z/(double)width);
                                values("point-"+cycle+"-"+cx+"-"+cy+"-"+cz+"-"+x+"-"+y+"-"+z,chunk);
                            }
                        }
                    }
                    method("Chunk.y",type("Chunk"),int.class,double.class).invoke(chunk,sy-1,-0.125);
                    method("Chunk.x",type("Chunk"),int.class,double.class).invoke(chunk,sx+width,1.125);
                    method("Chunk.z",type("Chunk"),int.class,double.class).invoke(chunk,sz+width,1.25);
                    values("fallback",chunk);
                }
                operation("swap","Chunk.swap",NONE);state("swap");
            }
            fillArray("flat-bulk",functions[2],chunk,length);fillArray("cell-bulk",functions[6],chunk,length);
            fillArray("interpolated-bulk",functions[3],chunk,length);fillArray("cell-bulk-repeat",functions[6],chunk,length);
            values("bulk-owner",chunk);state("bulk");
            if (mode>=3) {
                fillArray("graph-long",functions[8],chunk,length+1);
                fillArray("graph-long-repeat",functions[8],chunk,length+1);
            }
            operation("stop","Chunk.stop",NONE);operation("stop-twice","Chunk.stop",NONE);values("stopped",chunk);
        }
    }
    static void run(String[] f) throws Exception {
        NoiseChunkOracle t=new NoiseChunkOracle();
        t.width=Integer.parseInt(f[1]);t.height=Integer.parseInt(f[2]);t.minY=Integer.parseInt(f[3]);t.totalHeight=Integer.parseInt(f[4]);
        t.countXZ=Integer.parseInt(f[5]);t.firstX=Integer.parseInt(f[6]);t.firstZ=Integer.parseInt(f[7]);
        t.mode=Integer.parseInt(f[8]);t.style=Integer.parseInt(f[9]);t.cycles=Integer.parseInt(f[10]);t.seed=hex(f[11]);t.execute();
    }
}

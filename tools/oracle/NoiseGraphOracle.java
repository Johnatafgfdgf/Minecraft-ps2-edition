// Authored driver: mapAll, wrap, cache sharing and all evaluations run in the
// verified original JAR. No decompiled implementation is stored here.
package mcps2.oracle;
import java.lang.reflect.*;
import java.util.*;
import static mcps2.oracle.Oracle.*;

final class NoiseGraphOracle extends NoiseChunkOracle {
    Object root;
    long[] sourceTrace={0};
    void value(String label,Object context) throws Exception {
        emit("graph-value "+label+" "+sample(root,context)+" "+h64(sourceTrace[0]));
    }
    @Override void state(String label) throws Exception { trace=sourceTrace[0];super.state(label); }
    void array(String label,int length) throws Exception {
        double[] out=new double[length];int code=0;
        try { densityCall("Density.fill",root,new Class<?>[]{double[].class,type("ContextProvider")},out,chunk); }
        catch (Throwable e) { code=status(e); }
        long hash=0;for (double v:out) hash=(hash*0x100000001b3L)^Double.doubleToLongBits(v);
        emit("graph-array "+label+" "+code+" "+h64(hash)+" "+h64(sourceTrace[0]));state(label);
        value(label+"-owner",chunk);
    }
    void observe(Object source) throws Exception {
        constructChunk();((Map<?,?>)field("wrapped").get(chunk)).clear();
        Method wrap=type("Chunk").getDeclaredMethod(symbols.getProperty("Chunk.wrap"),type("Density"));wrap.setAccessible(true);
        Object visitor=Proxy.newProxyInstance(type("Visitor").getClassLoader(),new Class<?>[]{type("Visitor")},(proxy,m,args)-> {
            if (m.getName().equals(symbols.getProperty("Visitor.apply"))) return wrap.invoke(chunk,args[0]);
            if (m.isDefault()) return InvocationHandler.invokeDefault(proxy,m,args==null?new Object[0]:args);
            throw new UnsupportedOperationException(m.toString());
        });
        root=densityCall("Density.map",source,new Class<?>[]{type("Visitor")},visitor);
        Set<Object> unique=Collections.newSetFromMap(new IdentityHashMap<>());
        unique.addAll(((Map<?,?>)field("wrapped").get(chunk)).values());
        String counts="graph-caches";
        for (String name:new String[]{"ChunkInterpolated","ChunkFlat","ChunkColumn","ChunkOnce","ChunkCell"}) {
            int count=0;for (Object v:unique) if (type(name).isInstance(v)) ++count;counts+=" "+count;
        }
        emit(counts);
        emit("graph-bounds "+h64(Double.doubleToLongBits((double)densityCall("Density.min",root,NONE)))
            +" "+h64(Double.doubleToLongBits((double)densityCall("Density.max",root,NONE))));
        state("construct");value("inactive",chunk);
        for (int[] p:new int[][]{{firstX,minY,firstZ},{firstX+4,0,firstZ+4},{firstX-1,127,firstZ-1},
            {1875066,9,1875066},{Integer.MIN_VALUE,-1,Integer.MAX_VALUE}}) value("external",point(p[0],p[1],p[2]));
        int length=width*width*height;
        array("before",length);
        operation("initialize","Chunk.initialize",NONE);state("initialize");
        operation("advance","Chunk.advance",INT,0);state("advance");
        for (int cy=totalHeight/height-1;cy>=0;--cy) {
            operation("select","Chunk.select",new Class<?>[]{int.class,int.class},cy,0);state("select-"+cy);
            int sx=integer("cellStartBlockX"),sy=integer("cellStartBlockY"),sz=integer("cellStartBlockZ");
            for (int y=height-1;y>=0;--y) {
                method("Chunk.y",type("Chunk"),int.class,double.class).invoke(chunk,sy+y,y/(double)height);
                for (int x=0;x<width;++x) {
                    method("Chunk.x",type("Chunk"),int.class,double.class).invoke(chunk,sx+x,x/(double)width);
                    for (int z=0;z<width;++z) {
                        method("Chunk.z",type("Chunk"),int.class,double.class).invoke(chunk,sz+z,z/(double)width);
                        value("point-"+cy+"-"+x+"-"+y+"-"+z,chunk);value("repeat",chunk);
                    }
                }
            }
            array("cell",length);array("long",length+1);array("long-repeat",length+1);
        }
        operation("swap","Chunk.swap",NONE);state("swap");
        operation("stop","Chunk.stop",NONE);state("stop");value("stopped",chunk);
        operation("restart","Chunk.initialize",NONE);state("restart");operation("restop","Chunk.stop",NONE);
    }
    static void run(String[] f,boolean data) throws Exception {
        NoiseGraphOracle t=new NoiseGraphOracle();int layout=Integer.parseInt(f[data?2:3]);
        t.width=4;t.height=8;t.minY=-64;t.totalHeight=16;t.countXZ=1;
        t.firstX=new int[]{-32,12800,-30000000}[layout%3];t.firstZ=new int[]{64,-9280,29999968}[layout%3];
        Object source=data?densityDataField(f):densityNodes(f,t.sourceTrace,false)[Integer.parseInt(f[2])];
        t.observe(source);
    }
}

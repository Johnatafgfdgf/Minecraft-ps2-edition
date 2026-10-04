// Authored reflection harness. It invokes the user's local original JAR; it does not reimplement its algorithms.
package mcps2.oracle;
import java.io.*;
import java.lang.reflect.*;
import java.nio.file.*;
import java.util.*;

public final class Oracle {
    static final Properties symbols = new Properties();
    static final PrintStream observations = System.out; // Bootstrap redirects System.out to its logger.
    static int caseIndex = 0;
    static Class<?> type(String key) throws Exception { return Class.forName(symbols.getProperty(key)); }
    static Method method(String key, Class<?> owner, Class<?>... parameters) throws Exception {
        return owner.getMethod(symbols.getProperty(key), parameters);
    }
    static Object call(String key, Object target, Class<?>[] parameters, Object... arguments) throws Exception {
        return method(key, target.getClass(), parameters).invoke(target, arguments);
    }
    static Object staticCall(String key, String owner, Class<?>[] parameters, Object... arguments) throws Exception {
        return method(key, type(owner), parameters).invoke(null, arguments);
    }
    static long hex(String value) { return Long.parseUnsignedLong(value, 16); }
    static String h64(long value) { return String.format(Locale.ROOT, "%016x", value); }
    static String h32(int value) { return String.format(Locale.ROOT, "%08x", value); }
    static void emit(String value) { observations.println("R\t" + caseIndex + "\t" + value); }
    static final Class<?>[] NONE = new Class<?>[0];
    static final Class<?>[] INT = {int.class};
    static final Class<?>[] LONG = {long.class};
    static final Class<?>[] XYZ = {int.class, int.class, int.class};
    static boolean statesInitialized = false;

    static String javaString(String token) {
        if (token.equals("-")) return "";
        char[] units=new char[token.length()/4];
        for (int i=0;i<units.length;++i) units[i]=(char)Integer.parseInt(token.substring(i*4,i*4+4),16);
        return new String(units);
    }
    static void factoryDraw(String label, Object child) throws Exception {
        int a=(int)call("Random.int",child,NONE);
        long b=nextLong(child);
        double c=(double)call("Random.double",child,NONE);
        int d=(int)call("Random.bounded",child,INT,1073741825);
        emit(label + " " + h32(a) + " " + h64(b) + " " + h64(Double.doubleToRawLongBits(c)) + " " + d);
    }
    static void factory(String[] f) throws Exception {
        long seed=hex(f[2]); Object source=random(f[1],seed);
        Object factory=call("Random.positional",source,NONE);
        emit("factory-parent " + h64(nextLong(source)));
        if (f[1].startsWith("worldgen-")) emit("factory-count " + call("Worldgen.count",source,NONE));
        if (f[0].equals("factory-hash")) {
            String text=javaString(f[3]);
            Object hash=staticCall("Support.hash","Support",new Class<?>[]{String.class},text);
            emit("hash " + h64((long)call("Seed128.low",hash,NONE)) + " " + h64((long)call("Seed128.high",hash,NONE)));
            Object child=method("Factory.hash",type("Factory"),String.class).invoke(factory,text);
            factoryDraw("factory-hash",child); return;
        }
        for (int i=0;i<Integer.parseInt(f[3]);++i) {
            int x=i*0x9e3779b9+0x11221122, y=i*1664525+54321, z=i*0x7f4a7c15+0x13579bdf;
            if (i<4) { x=Integer.MIN_VALUE+i; y=-64+i; z=Integer.MAX_VALUE-i; }
            emit("position-seed " + h64((long)staticCall("Mth.seed","Mth",XYZ,x,y,z)));
            Object child=method("Factory.at",type("Factory"),XYZ).invoke(factory,x,y,z);
            factoryDraw("factory-at",child);
            child=method("Factory.seed",type("Factory"),LONG).invoke(factory,seed^(long)(i*0x9e3779b9));
            factoryDraw("factory-seed",child);
        }
    }

    static void noise(String[] fields) throws Exception {
        Object source=random(fields[1],hex(fields[2]));
        Object noise=type("Noise").getConstructor(type("RandomSource")).newInstance(source);
        double xo=type("Noise").getField(symbols.getProperty("Noise.xo")).getDouble(noise);
        double yo=type("Noise").getField(symbols.getProperty("Noise.yo")).getDouble(noise);
        double zo=type("Noise").getField(symbols.getProperty("Noise.zo")).getDouble(noise);
        emit("offsets " + h64(Double.doubleToRawLongBits(xo)) + " " + h64(Double.doubleToRawLongBits(yo)) + " " + h64(Double.doubleToRawLongBits(zo)));
        emit("noise-rng " + h64(nextLong(source)));
        for (int i=0;i<Integer.parseInt(fields[3]);++i) {
            double x,y,z;
            if (i<8) { x=-xo+(i-4)/8.0; y=-yo+(i%3)/4.0; z=-zo+(i%5)/8.0; }
            else {
                x=(int)(i*0x9e3779b9+0x11221122)/32.0;
                y=((int)(i*1664525+54321)%1024)/8.0;
                z=(int)(i*0x7f4a7c15+0x13579bdf)/64.0;
            }
            double value=(double)call("Noise.value",noise,new Class<?>[]{double.class,double.class,double.class},x,y,z);
            emit("noise3 " + i + " " + h64(Double.doubleToRawLongBits(value)));
            double scale=(i%7)/16.0,limit=i%3!=0 ? (i%5)/8.0 : -1.0;
            value=(double)call("Noise.step",noise,new Class<?>[]{double.class,double.class,double.class,double.class,double.class},x,y,z,scale,limit);
            emit("noise5 " + i + " " + h64(Double.doubleToRawLongBits(value)));
        }
    }

    static void state(String[] fields) throws Exception {
        if (!statesInitialized) {
            staticCall("Shared.detect", "Shared", NONE);
            staticCall("Bootstrap.boot", "Bootstrap", NONE);
            statesInitialized = true;
        }
        Object map=type("Block").getField(symbols.getProperty("Block.stateMap")).get(null);
        Object state=call("IdMap.byId",map,INT,Integer.parseInt(fields[1]));
        if (state==null) { emit("state -1"); return; }
        Collection<?> properties=(Collection<?>)call("State.properties",state,NONE);
        for (Object property:properties) {
            // A concrete property may be package-private; invoke its public base class.
            String name=(String)method("Property.name",type("Property")).invoke(property);
            if (!name.equals(fields[2])) continue;
            Optional<?> value=(Optional<?>)method("Property.parse",type("Property"),String.class).invoke(property,fields[3]);
            if (value.isEmpty()) { emit("state -1"); return; }
            Object next=method("State.set",type("StateHolder"),type("Property"),Comparable.class).invoke(state,property,value.get());
            int id=(int)staticCall("Block.stateId","Block",new Class<?>[]{type("BlockState")},next);
            emit("state " + id); return;
        }
        emit("state -1");
    }

    static Object random(String variant, long seed) throws Exception {
        if (variant.startsWith("worldgen-")) {
            Object base = random(variant.substring(9), seed);
            return type("Worldgen").getConstructor(type("RandomSource")).newInstance(base);
        }
        return type(variant.equals("legacy") ? "Legacy" : "Xoro").getConstructor(long.class).newInstance(seed);
    }
    static long nextLong(Object source) throws Exception { return (long)call("Random.long", source, NONE); }

    static void rng(String[] fields) throws Exception {
        Object source = random(fields[1], hex(fields[2]));
        for (int bound : new int[]{0, -1}) {
            boolean rejected = false;
            try { call("Random.bounded", source, INT, bound); }
            catch (InvocationTargetException error) {
                if (!(error.getCause() instanceof IllegalArgumentException)) throw error;
                rejected = true;
            }
            emit("reject " + (rejected ? 1 : 0));
        }
        int[] bounds = {1,2,3,16,17,1000,1073741825,Integer.MAX_VALUE};
        int count = Integer.parseInt(fields[3]);
        for (int i = 0; i < count; ++i) {
            if (i == 127) call("Random.seed", source, LONG, hex(fields[2]) ^ 0xfedcba9876543210L);
            String result;
            switch (i % 6) {
                case 0: result = "I " + h32((int)call("Random.int", source, NONE)); break;
                case 1: result = "L " + h64(nextLong(source)); break;
                case 2: result = "B " + call("Random.bounded", source, INT, bounds[(i / 6) % bounds.length]); break;
                case 3: result = "F " + h32(Float.floatToRawIntBits((float)call("Random.float", source, NONE))); break;
                case 4: result = "D " + h64(Double.doubleToRawLongBits((double)call("Random.double", source, NONE))); break;
                default: result = "O " + ((boolean)call("Random.boolean", source, NONE) ? 1 : 0);
            }
            emit("rng " + i + " " + result);
        }
    }

    static void position(String[] f) throws Exception {
        int x = Integer.parseInt(f[1]), y = Integer.parseInt(f[2]), z = Integer.parseInt(f[3]);
        long block = (long)staticCall("Block.pack", "BlockPos", XYZ, x,y,z);
        long section = (long)staticCall("Section.pack", "SectionPos", XYZ, x,y,z);
        emit("block " + h64(block) + " " + staticCall("Block.x", "BlockPos", LONG, block)
            + " " + staticCall("Block.y", "BlockPos", LONG, block) + " " + staticCall("Block.z", "BlockPos", LONG, block));
        emit("section " + h64(section) + " " + staticCall("Section.x", "SectionPos", LONG, section)
            + " " + staticCall("Section.y", "SectionPos", LONG, section) + " " + staticCall("Section.z", "SectionPos", LONG, section));
        Object pos = type("BlockPos").getConstructor(XYZ).newInstance(x,y,z);
        int relative = Short.toUnsignedInt((short)staticCall("Section.relative", "SectionPos", new Class<?>[]{type("BlockPos")}, pos));
        emit("local " + staticCall("Section.floor", "SectionPos", INT, x) + " " + relative);
        emit("chunk " + h64((long)staticCall("Chunk.pack", "ChunkPos", new Class<?>[]{int.class,int.class}, x,z)));
    }

    static void storage(String[] f) throws Exception {
        int bits = Integer.parseInt(f[1]), size = Integer.parseInt(f[2]), seed = (int)hex(f[3]);
        int mask = bits == 32 ? 0x7fffffff : (1 << bits) - 1;
        Object data = type("Storage").getConstructor(int.class,int.class).newInstance(bits,size);
        for (int i=0; i<size; ++i) call("Storage.set", data, new Class<?>[]{int.class,int.class}, i,(i*0x9e3779b9+seed)&mask);
        long checksum = 0;
        for (int i=size-1; i>=0; i-=3) {
            int previous = (int)call("Storage.swap", data, new Class<?>[]{int.class,int.class}, i,((i*1664525+seed)^0x5a5a5a5a)&mask);
            checksum ^= (long)previous * (i+1L);
        }
        emit("previous " + h64(checksum));
        checksum = 0;
        for (int i=0; i<size; ++i) checksum = checksum*0x100000001b3L ^ Integer.toUnsignedLong((int)call("Storage.get", data, INT, i));
        emit("read " + h64(checksum));
        long[] raw = (long[])call("Storage.raw", data, NONE);
        for (int i=0; i<raw.length; ++i) emit("word " + i + " " + h64(raw[i]));
    }

    static void ticks(String[] f) throws Exception {
        int seed = (int)hex(f[1]), count = Integer.parseInt(f[2]);
        Object queue = type("TickQueue").getConstructor().newInstance();
        Object[] types = {new Object(),new Object(),new Object()};
        Class<?> posType=type("BlockPos"), priorityType=type("Priority"), tickType=type("Tick");
        Constructor<?> constructor=tickType.getConstructor(Object.class,posType,long.class,priorityType,long.class);
        int inserted = 0;
        for (int i=0; i<count; ++i) {
            seed ^= seed << 13; seed ^= seed >>> 17; seed ^= seed << 5;
            int logical=i%97, time=(int)(Integer.toUnsignedLong(seed)%80)-20;
            int priority=(int)(Integer.toUnsignedLong(seed)%7)-3;
            Object pos=posType.getConstructor(XYZ).newInstance(logical,-64,logical/7);
            Object entityType=types[i%3];
            boolean present=(boolean)call("TickQueue.contains",queue,new Class<?>[]{posType,Object.class},pos,entityType);
            if (!present) ++inserted;
            Object tick=constructor.newInstance(entityType,pos,(long)time,staticCall("Priority.byValue","Priority",INT,priority),(long)i);
            call("TickQueue.schedule",queue,new Class<?>[]{tickType},tick);
        }
        emit("inserted " + inserted);
        Object tick;
        while ((tick=call("TickQueue.poll",queue,NONE))!=null) {
            Object pos=call("Tick.pos",tick,NONE), p=call("Tick.priority",tick,NONE), t=call("Tick.type",tick,NONE);
            int typeId = t==types[0] ? 0 : t==types[1] ? 1 : 2;
            emit("tick " + h64((long)call("Block.instancePack",pos,NONE)) + " " + call("Tick.time",tick,NONE)
                + " " + call("Priority.value",p,NONE) + " " + call("Tick.order",tick,NONE) + " " + typeId);
        }
    }

    public static void main(String[] args) throws Exception {
        try (Reader reader=Files.newBufferedReader(Path.of(args[0]))) { symbols.load(reader); }
        try (BufferedReader input=Files.newBufferedReader(Path.of(args[1]))) {
            String line;
            while ((line=input.readLine())!=null) {
                String[] f=line.split(" ");
                switch(f[0]) {
                    case "rng": rng(f); break;
                    case "pos": position(f); break;
                    case "storage": storage(f); break;
                    case "ticks": ticks(f); break;
                    case "state": state(f); break;
                    case "noise": noise(f); break;
                    case "factory": case "factory-hash": factory(f); break;
                    case "mix": emit("mix " + h64((long)staticCall("Support.mix","Support",LONG,hex(f[1])))); break;
                    case "zero": {
                        Object source=type("Xoro").getConstructor(long.class,long.class).newInstance(0L,0L);
                        for (int i=0;i<Integer.parseInt(f[1]);++i) emit("zero " + h64(nextLong(source)));
                        break;
                    }
                    case "fork": {
                        Object source=random(f[1],hex(f[2]));
                        for (int i=0;i<Integer.parseInt(f[3]);++i) {
                            Object child=call("Random.fork",source,NONE);
                            emit("fork " + h64(nextLong(child)) + " " + h64(nextLong(source)));
                        }
                        break;
                    }
                    case "worldgen": {
                        Object r=random("worldgen-"+f[1],hex(f[2]));
                        long seed=hex(f[2]); int x=Integer.parseInt(f[3]),z=Integer.parseInt(f[4]),index=Integer.parseInt(f[5]),step=Integer.parseInt(f[6]),salt=Integer.parseInt(f[7]);
                        Class<?>[] params={long.class,int.class,int.class};
                        long derived=(long)call("Worldgen.decoration",r,params,seed,x,z);
                        emit("decoration " + h64(derived) + " " + h64(nextLong(r)));
                        call("Worldgen.feature",r,params,seed,index,step); emit("feature " + h64(nextLong(r)));
                        call("Worldgen.large",r,params,seed,x,z); emit("large " + h64(nextLong(r)));
                        call("Worldgen.salt",r,new Class<?>[]{long.class,int.class,int.class,int.class},seed,x,z,salt);
                        emit("salt " + h64(nextLong(r)) + " " + call("Worldgen.count",r,NONE));
                        break;
                    }
                    case "slime": {
                        Object r=staticCall("Worldgen.slime","Worldgen",new Class<?>[]{int.class,int.class,long.class,long.class},Integer.parseInt(f[2]),Integer.parseInt(f[3]),hex(f[1]),hex(f[4]));
                        emit("slime " + call("Random.bounded",r,INT,10) + " " + h64(nextLong(r)));
                        break;
                    }
                    default: throw new IllegalArgumentException("Unknown case " + line);
                }
                ++caseIndex;
            }
        }
    }
}

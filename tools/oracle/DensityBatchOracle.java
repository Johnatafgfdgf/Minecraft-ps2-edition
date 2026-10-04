// Authored bulk provider. DensityFunction.fillArray executes in the verified
// original JAR; this harness supplies observations and input coordinates only.
package mcps2.oracle;
import java.lang.reflect.*;
import static mcps2.oracle.Oracle.*;

final class DensityBatchOracle {
    static long mix(long a,long b) { return a*0x100000001b3L ^ b; }
    static Object context(int i) throws Exception {
        int[] edges={-1024,-65,-64,-1,0,1,23,24,103,104,127,128,239,240,256,320};
        int y=i>=0 && i<16?edges[i]:((i*1664525+54321)%2048)-64;
        return type("Context").getConstructor(XYZ).newInstance((i*0x9e3779b9+0x11221122)%30000001,y,(i*0x7f4a7c15+0x13579bdf)%30000001);
    }
    static void observe(Object root,int count,long[] trace) throws Exception {
        long[] calls={0,0};Class<?> providerType=type("ContextProvider");
        Object provider=Proxy.newProxyInstance(providerType.getClassLoader(),new Class<?>[]{providerType},(proxy,m,args)-> {
            if (m.getName().equals(symbols.getProperty("Provider.index")) && m.getParameterCount()==1) {
                int i=(int)args[0];trace[0]=mix(trace[0],0xe100000000000000L|Integer.toUnsignedLong(i));++calls[0];return context(i);
            }
            if (m.getName().equals(symbols.getProperty("Provider.direct")) && m.getParameterCount()==2) {
                double[] out=(double[])args[0];Object field=args[1];
                trace[0]=mix(trace[0],0xe000000000000000L|out.length);++calls[1];
                for (int i=0;i<out.length;++i) {
                    trace[0]=mix(trace[0],0xe200000000000000L|i);
                    out[i]=(double)densityCall("Density.value",field,new Class<?>[]{type("FunctionContext")},context(i));
                }
                return null;
            }
            throw new UnsupportedOperationException(m.toString());
        });
        trace[0]=0;double[] out=new double[count];
        densityCall("Density.fill",root,new Class<?>[]{double[].class,providerType},out,provider);
        for (int i=0;i<count;++i) emit("density-batch "+i+" "+h64(Double.doubleToLongBits(out[i])));
        emit("density-batch-trace "+h64(trace[0])+" "+calls[0]+" "+calls[1]);
    }
}

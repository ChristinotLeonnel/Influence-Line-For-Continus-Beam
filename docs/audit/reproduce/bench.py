import sys, time, psutil, os, Tsaraloha.LIPoutreContinue as m
L=[float(x) for x in sys.argv[1].split(",")]; st=float(sys.argv[2]); env=sys.argv[3]=="1"
p=psutil.Process(); r0=p.memory_info().rss
t=time.perf_counter(); o=m.Output(E=[210e9]*len(L),I=[8e-4]*len(L),L=L,steps=st); t_ctor=time.perf_counter()-t
t=time.perf_counter(); o.compute(); t_c=time.perf_counter()-t
t=time.perf_counter(); bm=o.BM; t_acc=time.perf_counter()-t
N=len(o.X)
t_env=float('nan')
if env:
    o.set_loads(point_loads=[m.Load(intensity=[60,120,120,60],length=[0,1.5,4.5,1.5],name="T")],
                distrib_loads=[m.Load(intensity=[10.0],length=[0,8.0],name="q")])
    t=time.perf_counter(); o.compute_load_envelopes(); t_env=time.perf_counter()-t
mi=p.memory_full_info(); peak=getattr(mi,'peak_wset',mi.rss)
print(f"{m._BACKEND:6s} N={N:5d} ctor={t_ctor:7.3f}s compute={t_c:7.3f}s BM-access={t_acc:6.3f}s envelopes={t_env:8.2f}s peakRSS={(peak-r0)/2**20:8.1f} MiB")

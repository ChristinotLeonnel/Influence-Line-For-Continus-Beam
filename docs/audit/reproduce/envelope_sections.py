import numpy as np, Tsaraloha.LIPoutreContinue as m
# --- shear misalignment: section x=5 on simple span L=10, 2 axles 100 kN @ 3 m
L=10.0; o=m.Output(E=[1.0],I=[1.0],L=[L],steps=1.0); o.compute()
ld=m.Loading(curves=o.SF, position=o.X, span_node_positions=o.span_node_positions, spans=o.L_spans,
             point_loads=[m.Load(intensity=[100.0,100.0],length=[0.0,3.0],name="T")], distrib_loads=[])
sec=5; lib=ld.plural_point_load([100.0,100.0],[0.0,3.0],0,sec)
def V(x,a,side): return (-a/L) if (a<x or (a==x and side>0)) else 1-a/L
best=0
for a0 in o.X:
  for side in (-1,1):
    v=sum(100*V(5.0,a0+d,side) for d in (0,3) if a0+d<=L)
    best=v if abs(v)>abs(best) else best
print(f"shear @x=5 lib={lib['value']:.3f} exact={best:.3f}  (SF row len {len(o.SF[0][sec])} vs X len {len(o.X)})")
# --- per-section sign cancellation, 2 spans, A=single 100 kN, B=2x60 kN @ 9 m
o=m.Output(E=[1.0]*2,I=[1.0]*2,L=[10.0,10.0],steps=0.5); o.compute()
loads=[([100.0],[0.0]),([60.0,60.0],[0.0,9.0])]
ld=m.Loading(curves=o.BM, position=o.X, span_node_positions=o.span_node_positions, spans=o.L_spans,
             point_loads=[m.Load(intensity=i,length=l,name=f"L{k}") for k,(i,l) in enumerate(loads)], distrib_loads=[])
X=np.array(o.X); worst=0; where=None
for s in range(2):
  for sec in range(len(o.span_node_positions[s])):
    il=np.array(o.BM[s][sec]); pos=[];neg=[]
    for I,Ls in loads:
      offs=np.cumsum(Ls); vals=[]
      for a0 in np.concatenate([X, X-offs[-1]]):     # allow entry from the left
        v=sum(p*np.interp(a0+d,X,il) for p,d in zip(I,offs) if 0<=a0+d<=X[-1]); vals.append(v)
      pos.append(max(vals)); neg.append(min(vals))
    true=max(sum(pos),-sum(neg))
    libv=abs(sum(ld.plural_point_load(I,Ls,s,sec)['value'] for I,Ls in loads))
    if true-libv>worst: worst=true-libv; where=(s,sec,o.span_node_positions[s][sec],libv,true)
print("largest per-section under-estimate of |M| envelope: span %d sec %d x=%.2f lib=%.2f true=%.2f"%where)

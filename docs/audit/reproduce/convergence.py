"""Convergence of UDL effect = trapezoidal integral of the library's influence line."""
import numpy as np, Tsaraloha.LIPoutreContinue as m
L=10.0; exact=-L*L/8          # 2 equal spans, q=1 on both: M_B = -qL^2/8
print(" step      M_B(trapz)        rel.err     order")
prev=None
for h in [2.0,1.0,0.5,0.25,0.125,0.0625]:
    o=m.Output(E=[1.0]*2,I=[1.0]*2,L=[L,L],steps=h); o.compute()
    X=np.array(o.X); il=np.array(o.support_moment[1])
    v=np.trapezoid(il,X); e=abs(v-exact)/abs(exact)
    print(f"{h:6.4f}  {v:16.12f}  {e:10.3e}  {'' if prev is None else f'{np.log2(prev/e):5.2f}'}"); prev=e

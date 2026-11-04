import json, sys, Tsaraloha.LIPoutreContinue as m
o=m.Output(E=[210e9,210e9,210e9],I=[8e-4,1.2e-3,8e-4],L=[6.0,8.0,11.0],steps=0.5); o.compute()
o.set_loads(point_loads=[m.Load(intensity=[60,120],length=[0,1.5],name="T")],distrib_loads=[m.Load(intensity=[10.0,5.0],length=[0,3.0,4.0],name="q")]); o.compute_load_envelopes()
d={"BM":o.BM,"SF":o.SF,"Def":o.Def,"SM":o.support_moment,"X":o.X,
   "env":{k:getattr(o,k) for k in dir(o) if k.endswith("_envelope")},
   "max":{k:getattr(o,k) for k in dir(o) if k.endswith("_max_positions")}}
json.dump(d,open(sys.argv[1],"w"),default=lambda x: x if not hasattr(x,'to_dict') else x.to_dict())

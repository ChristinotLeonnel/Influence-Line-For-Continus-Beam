import Tsaraloha.LIPoutreContinue as lipc 

L = [15,15] 
E = [30e9,30e9]
I = [7e-6,7e-6] 

P1 = lipc.Output(E,I,L,0.5) 
P1.compute() 




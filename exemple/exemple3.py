from Tsaraloha.LIPoutreContinue import Output as opt 
import matplotlib.pyplot as plt 

E = 3e9 
I = 1e-6 
E_t,I_t,L_t, pas_t = [], [], [20,25,20], 1

for i in L_t: 
    E_t.append(E)
    I_t.append(I)

Poutre = opt(E_t,I_t,L_t, pas_t)    
Poutre.compute(True) 

SM = Poutre.support_moment 
BM = Poutre.BM 
X = Poutre.X 

plt.plot(X,SM[1], "k--")

plt.plot(X,BM[1][0])  
plt.show() 
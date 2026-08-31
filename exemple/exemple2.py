from Tsaraloha.LIPoutreContinue import Output as opt 
from Tsaraloha.LIPoutreContinue import Loading as ldg  
from Tsaraloha.LIPoutreContinue import Load as ld

import matplotlib.pyplot as plt

Inertie = 0.3*0.6**3/12   # Iz =  bxh^3/12 
Young = 30e9              # E Beton de 30GPa   
pas = 1                 # Pas de 0.1 m 
L, E, I = [0, 154, 15, 0], [], [] # L en m 

for i in L:
    E.append(Young)
    I.append(Inertie)
# PoutreCentrale ==> PC

PC = opt(E,I,L,pas)
PC.compute()

ROT = PC.Rot
BM = PC.BM
SF = PC.SF
DEF = PC.Def
SP = PC.support_moment 

X = PC.X
XSF = PC.shear_force_all_abscissa 

def Trace(abscisse , courbe):
    try:
        for i,j in zip(abscisse,courbe):
            for k,l in zip(i,j):
                plt.plot(k,l)
        plt.plot(abscisse[0][0],[0 for i in abscisse[0][0]],"k--")
    except:
        try:
            for i in courbe:
                for j in i:
                    plt.plot(abscisse,j)
            plt.plot(abscisse,[0 for i in abscisse],"k--")
        except:
            for i in courbe:
                plt.plot(abscisse,i) 
            plt.plot(abscisse,[0 for i in abscisse],"k--")

BC1 = ld([6,12,12], [0,2.5,7],"Convoi 1")  
BC2 = ld([6,15,12], [0,2.5,8],"Convoi 2")  
BC3 = ld([6,14,12], [0,2.5,5],"Convoi 3")  
BC4 = ld([6,12,12], [0,2.5,5,4],"Convoi 4")  

Ch = ldg(BM, X, PC.span_node_positions, PC.L_spans, [BC1,BC2,BC3], [BC4]) 
q = Ch.plural_point_load([6,12,12],[0,4,2],0,1)
print(q) 

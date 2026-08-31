import Tsaraloha.LIPoutreContinue as lipc 
import matplotlib.pyplot as plt 

Inertie = 0.3*0.6**3/12   # Iz =  bxh^3/12 
Young = 30e9              # E Beton de 30GPa   
pas = 0.1                 # Pas de 0.1 m 
L, E, I = [0, 15, 15, 0], [], [] # L en m 

for i in L:
    E.append(Young)
    I.append(Inertie)
# PoutreRive ==> PR
PR = lipc.Hyperstatique(E,I,L,pas)

SOUPLESSE = {
    "a" : PR.a_spans,
    "b" : PR.b_spans,
    "c" : PR.c_spans 
             }
print(SOUPLESSE)  

FOYER = {"phy" : PR.phy,"phy_prime" : PR.phy_prime}
print(FOYER) 

SM = PR.support_moment

SF = PR.shear_force(False)
XSF = PR.shear_force(True) 

BM = PR.bending_moments()
DEF = PR.deflection()
ROT = PR.rotation() 
X = PR.points_x_coordinates(PR.span_node_positions)

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
        
Trace(X,SM)
plt.show() 

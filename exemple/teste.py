a = [1,2,3]
b = []

compt = 0
for i in a:
    compt = compt + i 
    b.append(compt) 

print(b) 

compt = 0
for idx,i in enumerate(a):
    a[idx] = i + compt
    compt = a[idx]
print(a)   
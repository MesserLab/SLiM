import msprime
import sys
import io

nodes = open("NodeTable.txt","r")
edges = open("EdgeTable.txt","r")

tr = msprime.load_text(nodes,edges)
#print(type(tr))
unsimplified = tr.trees()

samples = [] 
for i in range(10):
	samples.append(90 + i)

tr = tr.simplify(samples)

simplified = tr.trees()

for i in unsimplified:
	print(i.draw(format="unicode"))

print("------------------- SIMPLIFIED TREES --------------------"+"\n")

for i in simplified:
	print(i.draw(format="unicode"))



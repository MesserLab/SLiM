import msprime
import sys
import io

nodes = open("NodeTable.txt","r")
edges = open("EdgeTable.txt","r")

Nodes = msprime.parse_nodes(nodes)
Edges = msprime.parse_edges(edges)

msprime.sort_tables(nodes=Nodes,edges=Edges)

minimumTime = min(Nodes.time)

samples = []
for i,j in enumerate(Nodes.time):
	if (j == minimumTime):
		samples.append(i)
	
msprime.simplify_tables(samples,Nodes,Edges)

Nodes.set_columns(flags=Nodes.flags,time=(Nodes.time - minimumTime),population=Nodes.population)
treeSequence = msprime.load_tables(nodes=Nodes,edges=Edges)

for tree in treeSequence.trees():
	print(tree.draw(format="unicode"))

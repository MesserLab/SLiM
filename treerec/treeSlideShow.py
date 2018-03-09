'''
This Script play a short slide show of the gene trees by printing them to stdout.  
Must have the Node and Edge Tables to read in.

usage python3 treeSlideShow.py Nodetablefile Edgetabefile

'''



from termcolor import colored
import time
import msprime
import sys
import io

node_file = open(sys.argv[1],"r")
edge_file = open(sys.argv[2],"r")

Nodes = msprime.parse_nodes(node_file,base64_metadata=False)
Edges = msprime.parse_edges(edge_file)

minimumTime = min(Nodes.time)

samples = []
for i,j in enumerate(Nodes.time):
	if (j == minimumTime):
		samples.append(i)
	
msprime.simplify_tables(samples,Nodes,Edges)

Nodes.set_columns(flags=Nodes.flags,time=(Nodes.time - minimumTime),population=Nodes.population)
treeSequence = msprime.load_tables(nodes=Nodes,edges=Edges)

string1 = "-. .-.   .-. .-.   .-. .-.   .-. .-.   .-. .-.   .-. .-.   .-. .-.   .-. .-.   .-. .-.   .-. .-.   .-. .-.   .-. .-.   .-. .-.   .-. .-.   .-. .-.   .-. .-.   .-. .-.   ."
string2 = "||\|||\ /|||\|||\ /|||\|||\ /|||\|||\ /|||\|||\ /|||\|||\ /|||\|||\ /|||\|||\ /|||\|||\ /|||\|||\ /|||\|||\ /|||\|||\ /|||\|||\ /|||\|||\ /|||\|||\ /|||\|||\ /|||\|||\ /|"
string3 = "|/ \|||\|||/ \|||\|||/ \|||\|||/ \|||\|||/ \|||\|||/ \|||\|||/ \|||\|||/ \|||\|||/ \|||\|||/ \|||\|||/ \|||\|||/ \|||\|||/ \|||\|||/ \|||\|||/ \|||\|||/ \|||\|||/ \|||\||"
string4 = "    `-  `-`   `-  `-`   `-  `-`   `-  `-`   `-  `-`   `-  `-`   `-  `-`   `-  `-`   `-  `-`   `-  `-`   `-  `-`   `-  `-`   `-  `-`   `-  `-`   `-  `-`   `-  `-`   `-  `-"


dna_length = len(string1)

for tree in treeSequence.trees():
	
	time.sleep(0.25)

	st = '\n' * 100
	print(st)

	print(tree.draw(format="unicode",height = 200))
	print(tree.interval)
	for i in range(6):
		print()

	percentFirstInterval = tree.interval[0]/treeSequence.sequence_length
	percentSecondInterval = tree.interval[1]/treeSequence.sequence_length
	
	percentDNA = int(percentFirstInterval * dna_length)
	percentDNA2 = int(percentSecondInterval * dna_length) 

	print("                "+colored(string1[0:percentDNA],"red") + colored(string1[percentDNA:percentDNA2],'blue') + string1[percentDNA2:])
	print("                "+colored(string2[0:percentDNA],"red") + colored(string2[percentDNA:percentDNA2],'blue') + string2[percentDNA2:])
	print("                "+colored(string3[0:percentDNA],"red") + colored(string3[percentDNA:percentDNA2],'blue') + string3[percentDNA2:])
	print("                "+colored(string4[0:percentDNA],"red") + colored(string4[percentDNA:percentDNA2],'blue') + string4[percentDNA2:])
	
	print("")
	

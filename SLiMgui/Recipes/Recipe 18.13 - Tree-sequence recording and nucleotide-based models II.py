# Keywords: Python, nucleotide-based, nucleotide sequence, sequence-based mutation rate

import pyslim
import numpy as np

ts = pyslim.load("recipe_nucleotides.trees")

M = [[0 for _ in pyslim.NUCLEOTIDES] for _ in pyslim.NUCLEOTIDES]
for mut in ts.mutations():
    mut_list = mut.metadata["mutation_list"]
    k = np.argmax([u["slim_time"] for u in mut_list])
    derived_nuc = mut_list[k]["nucleotide"]
    if mut.parent == -1:
        acgt = ts.reference_sequence.data[int(ts.site(mut.site).position)]
        parent_nuc = pyslim.NUCLEOTIDES.index(acgt)
    else:
        parent_mut = ts.mutation(mut.parent)
        assert(parent_mut.site == mut.site)
        parent_nuc = parent_mut.metadata["mutation_list"][0]["nucleotide"]
    M[parent_nuc][derived_nuc] += 1

print("{}\t{}\t{}".format('ancestral', 'derived', 'count'))
for j, a in enumerate(pyslim.NUCLEOTIDES):
    for k, b in enumerate(pyslim.NUCLEOTIDES):
        print("{}\t{}\t{}".format(a, b, M[j][k]))
